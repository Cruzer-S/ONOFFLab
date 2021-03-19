#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <time.h>

#include <wiringPi.h>
#include <wiringSerial.h>

#include "wifi_manager.h"
#include "task_manager.h"

#define stringify(x) #x
#define UNION_LIBRARY(NAME) stringify(u_ ## NAME)

#include UNION_LIBRARY(utils.h)
#include UNION_LIBRARY(ipc_manager.h)
#include UNION_LIBRARY(logger.h)

#define BOAD_RATE			9600
#define SERIAL_DEVICE		"/dev/ttyS0"
#define DEVICE_ID			0x00000001

#define HEADER_SIZE			1024

#define SERVER_DOMAIN		"www.mythos.ml"
#define SERVER_PORT			1584

bool is_initiate(int serial);
int parse_data(int serial, char *ssid, char *psk);
int wait_command(int sock);
int32_t handling_command(int sock, int commnad, struct task_manager *task);

int ipc_to_target(int sock, enum IPC_COMMAND cmd, ...);
int ipc_receive_request(int sock);

int main(int argc, char *argv[])
{
	int bluetooth_port, serv_sock;
	int32_t dev_id;
	struct task_manager *task_manager;

	if (wiringPiSetup() == -1)
		logg(LOG_CRI, "unable to start wiringPi: %s", strerror(errno));

	logg(LOG_INF, "setup wiring pi");

	if ((bluetooth_port = serialOpen(SERIAL_DEVICE, BOAD_RATE)) < 0)
		logg(LOG_CRI, "failed to open %s serial: %s",
				       SERIAL_DEVICE, strerror(errno));

	logg(LOG_INF, "open bluetooth port");

	task_manager = create_task_manager(MAX_TASK);
	if (task_manager == NULL)
		logg(LOG_CRI, "create_task_manager() error");

	logg(LOG_INF, "craete task manager");

	do {
		const char *host;
		uint16_t port;
		long check;

		check = (argc > 2) ? strtol(argv[2], NULL, 10) : SERVER_PORT;
		if (check < 0 || check > USHRT_MAX)
			logg(LOG_CRI, "port number out of range", check);

		port = (uint16_t) check;
		host = (argc > 2) ? argv[1] : SERVER_DOMAIN;

		logg(LOG_INF, "host: %s\tport: %hu", host, port);

		if ((serv_sock = connect_to_target(host, port)) < 0)
			logg(LOG_CRI, "connect_to_target() error", host, port);

		logg(LOG_INF, "connect to target");
	} while (false);

	do {
		dev_id = (argc == 4) ? strtol(argv[3], NULL, 10) : DEVICE_ID;
		if (ipc_to_target(serv_sock, IPC_REGISTER_DEVICE, dev_id) < 0)
			logg(LOG_CRI, "ipc_to_target(IPC_REGISTER_DEVICE) error");

		logg(LOG_INF, "register device to server %d", dev_id);
	} while (false);

	while (true) {
		// ========================================================================
		// checking whether the bluetooth data is available.
		// ========================================================================
		if (is_initiate(bluetooth_port)) {
			char ssid[SSID_SIZ + 1];
			char psk[PSK_SIZ + 1];

			if (parse_data(bluetooth_port, ssid, psk)) {
				if (change_wifi(ssid, psk))
					serialPutchar(bluetooth_port, true);
				else
					serialPutchar(bluetooth_port, false);
			}
		}

		// ========================================================================
		// checking whether the server sends command
		// ========================================================================
		int command;
		if ((command = wait_command(serv_sock)) < 0) {
			if (serv_sock > 0) close(serv_sock);

			serv_sock = connect_to_target(NULL, 0);
			if (serv_sock < 0) continue;

			logg(LOG_INF, "reconnect to target", dev_id);

			if (ipc_to_target(serv_sock, IPC_REGISTER_DEVICE, dev_id) < 0)
				logg(LOG_ERR, "ipc_to_target(IPC_REGISTER_DEVICE) error");

			logg(LOG_INF, "re-register device to server %d", dev_id);
		} else {
			if (command == 0) {
				flush_socket(serv_sock);
				continue;
			}

			logg(LOG_INF, "request from server %d", command);
			int32_t result = handling_command(serv_sock, command, task_manager);

			logg(LOG_INF, "handling_command() %d", result);
			if (sendt(serv_sock, &result, sizeof(result), CPS) < 0) {
				logg(LOG_ERR, "failed to send result \n");
				close(serv_sock);
				serv_sock = -1;

				continue;
			}

			flush_socket(serv_sock);
		}

		// ========================================================================
		// Whatever you wants
		// =======================================================================
	}

	close(serv_sock);
	serialClose(bluetooth_port);

	return 0;
}

int parse_data(int serial, char *ssid, char *psk)
{
	int step = 0, ch;
	uint8_t ssid_size, psk_size;

	for (clock_t end, start = end = clock();
		 (end - start) < CPS;
		 end = clock())
	{
		if (serialDataAvail(serial) == -1)
			continue;

		ch = (uint8_t) serialGetchar(serial);

		switch (step) {
		case 0: ssid_size = ch;
				step++;
				break;

		case 1: psk_size = ch;
				step++;
				break;

		case 2: *ssid++ = ch;
				if (ssid_size-- <= 0) {
					*--ssid = '\0';
					step++;
				} else break;

		case 3: *psk++ = ch;
				if (psk_size-- <= 0) {
					*--psk = '\0';
					step++;
				} else break;

		case 4: return 1;
		}
	}

	return -1;
}

bool is_initiate(int serial)
{
	static const uint8_t key[] = { 0x12, 0x34, 0x56, 0x78, (uint8_t) '\0'};
	const uint8_t *ptr = key;

	if (!serialDataAvail(serial))
		return false;

	for (clock_t end, start = end = clock();
		 (end - start) < CPS;
		 end = clock())
	{
		if (serialDataAvail(serial) == -1)
			continue;

		if (serialGetchar(serial) != *ptr++)
			return false;

		if (!*ptr) return true;
	}

	return false;
}

int32_t wait_command(int sock)
{
	int32_t command = 0;
	int ret;

	if (sock < 0) return -2;

	if ((ret = recvt(sock, &command, sizeof(command), CPS)) < 0)
		return (ret == -2) ? 0 : -1;

	return command;
}

int32_t handling_command(int sock, int command, struct task_manager *tm)
{
	switch (command) {
	case IPC_REGISTER_DEVICE: break;
	case IPC_REGISTER_GCODE: {
		int32_t length, id, ret;
		char buffer[BUFSIZ], fname[FILENAME_MAX];
		FILE *fp;

		if ((id = make_task(tm)) < 0)
			return -1;

		task_name(id, fname);

		logg(LOG_INF, "name: %s", fname);

		fp = fopen(fname, "wb");
		if (fp == NULL)
			return -2;

		if (recvt(sock, &length, sizeof(length), CPS) < 0)
			return -3;

		logg(LOG_INF, "length: %d", length);
		for (int received = 0, to_read;
			 received < length; received += to_read)
		{
			if ((to_read = recvt(sock, buffer,
						   LIMITS(length - received, sizeof(buffer)), CPS)) < 0)
			{ ret = -4; break; }

			if (to_read < 0) { ret = -5; break; }

			if ((ret = fwrite(buffer, to_read, sizeof(char), fp) == to_read))
			{ ret = -6; break; }
		}

		fclose(fp);

		if (ret < 0) return ret;
		else logg(LOG_INF, "receive successfully");

		break;
	}}

	return 0;
}

int ipc_to_target(int sock, enum IPC_COMMAND cmd, ...)
{
	va_list args;
	uint8_t header[HEADER_SIZE], *hp = header;
	int32_t command = cmd;

	va_start(args, cmd);

	hp = ASSIGN(hp, command);

	switch (cmd) {
	case IPC_REGISTER_DEVICE: ;
		int32_t dev_id = va_arg(args, int32_t);
		hp = ASSIGN(hp, dev_id);
		break;

	default: break;
	}

	va_end(args);

	if (sendt(sock, header, sizeof(header), CPS) < 0)
		return -1;

	do {
		int32_t result;

		if (recvt(sock, &result, sizeof(result), CPS) < 0)
			return -2;

		if (result < 0)
			return -3 + result;
	} while (false);

	return 0;
}

int ipc_receive_request(int sock)
{
	int32_t command;

	if (recv(sock, &command, sizeof(command), MSG_DONTWAIT) != sizeof(command))
		return -1;

	return command;
}

