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
#include "debugger.h"
#include "ipc_manager.h"
#include "task_manager.h"

#define BOAD_RATE			9600
#define SERIAL_DEVICE		"/dev/ttyS0"
#define DEVICE_ID			0x00000001

#define SERVER_DOMAIN		"www.mythos.ml"
#define SERVER_PORT			1584

#define LIMITS(value, max) ((value) < (max) ? (value) : (max))

bool is_initiate(int serial);
int parse_data(int serial, char *ssid, char *psk);
int wait_command(int sock);
int32_t handling_command(int sock, int commnad, struct task_manager *task);

int main(int argc, char *argv[])
{
	int bluetooth_port, serv_sock;
	struct task_manager *task_manager;

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s", strerror(errno));

	if ((bluetooth_port = serialOpen(SERIAL_DEVICE, BOAD_RATE)) < 0)
		error_handling("failed to open %s serial: %s",
				       SERIAL_DEVICE, strerror(errno));

	task_manager = create_task_manager(MAX_TASK);
	if (task_manager == NULL)
		error_handling("create_task_manager() error");

	do {
		const char *host;
		uint16_t port;
		long check;

		check = (argc > 2) ? strtol(argv[2], NULL, 10) : SERVER_PORT;
		if (check < 0 || check > USHRT_MAX)
			error_handling("port number out of range", check);

		port = (uint16_t) check;
		host = (argc > 2) ? argv[1] : SERVER_DOMAIN;

		if ((serv_sock = connect_to_target(host, port)) < 0)
			error_handling("connect_to_target() error", host, port);
	} while (false);

	do {
		int dev_id = (argc == 4) ? strtol(argv[3], NULL, 10) : DEVICE_ID;
		if (ipc_to_target(serv_sock, IPC_REGISTER_DEVICE, dev_id) < 0)
			error_handling("ipc_to_target(IPC_REGISTER_DEVICE) error");
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

			if (ipc_to_target(serv_sock, IPC_REGISTER_DEVICE, DEVICE_ID) < 0)
				error_handling("ipc_to_target(IPC_REGISTER_DEVICE) error");
		} else {
			if (command == 0) continue;

			int32_t result = handling_command(serv_sock, command, task_manager);
			if (sendt(serv_sock, &result, sizeof(result), CPS) < 0) {
				fprintf(stderr, "failed to send result \n");
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
		return (ret == -2) ? -1 : 0;

	return command;
}

int32_t handling_command(int sock, int command, struct task_manager *tm)
{
	switch (command) {
	case IPC_REGISTER_DEVICE: break;
	case IPC_REGISTER_GCODE: {
		int32_t length, id, ret;
		char buffer[BUFSIZ];
		FILE *fp;

		if ((id = make_task(tm)) < 0)
			return -1;

		fp = fopen(task_name(id), "wb");
		if (fp == NULL)
			return -2;

		if (recvt(sock, &length, sizeof(length), CPS) < 0)
			return -2;

		for (int received = 0, to_read;
			 received < length; received += to_read)
		{
			if ((to_read = recvt(sock, buffer,
						   LIMITS(length - received, sizeof(buffer)), CPS)) < 0)
			{ ret = -4; break; }

			if (to_read < 0) { ret = -5; break; }

			if ((ret = fwrite(buffer, to_read, sizeof(char), fp) == to_read))
			{ ret = -5; break; }
		}

		fclose(fp);

		if (ret < 0) return ret;

		break;
	}}

