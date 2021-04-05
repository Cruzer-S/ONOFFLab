#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <wiringPi.h>
#include <wiringSerial.h>

#include "wifi_manager.h"
#include "task_manager.h"
#include "device_manager.h"

#define stringify(x) #x
#define UNION_LIBRARY(NAME) stringify(u_ ## NAME)

#include UNION_LIBRARY(utils.h)
#include UNION_LIBRARY(ipc_manager.h)
#include UNION_LIBRARY(logger.h)

#define SERVER_DOMAIN		"cloud.onofflab.tk"
#define SERVER_PORT			1584
#define HEADER_SIZE			1024

bool is_initiate(int serial);
int parse_data(int serial, char *ssid, char *psk);
int32_t wait_packet(int sock, struct packet_header *packet);
int32_t handling_command(int sock, struct task_manager *task);

int send_packet(int sock, ...);

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

	task_manager = create_task_manager(5);
	if (task_manager == NULL)
		logg(LOG_CRI, "create_task_manager() error");

	show_task(task_manager);

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
		uint8_t dev_key[DEVICE_KEY_SIZE];

		memset(dev_key, 0x00, DEVICE_KEY_SIZE);
		strncpy((char *)dev_key, DEVICE_KEY, DEVICE_KEY_SIZE);

		dev_id = (argc == 4) ? strtol(argv[3], NULL, 10) : DEVICE_ID;
		if (send_packet(serv_sock, IPC_REGISTER_DEVICE, dev_id, dev_key) < 0)
			logg(LOG_CRI, "send_packet(IPC_REGISTER_DEVICE) error");

		logg(LOG_INF, "register device to server (%d:%s)", dev_id, dev_key);
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
		struct packet_header packet;
		int32_t result;
		if ((result = wait_packet(serv_sock, &packet)) < 0) {
			if (serv_sock > 0) close(serv_sock);

			serv_sock = connect_to_target(NULL, 0);
			if (serv_sock < 0) continue;

			logg(LOG_INF, "reconnect to target", dev_id);

			if (send_packet(serv_sock, IPC_REGISTER_DEVICE, dev_id) < 0)
				logg(LOG_ERR, "send_packet(IPC_REGISTER_DEVICE) error");

			logg(LOG_INF, "re-register device to server %d", dev_id);
		} else {
			if (result == 0) continue;

			int32_t result = handling_command(serv_sock, task_manager);
			logg(LOG_INF, "handling_command() %d", result);

			if (sendt(serv_sock, &result, sizeof(result), CPS) <= 0) {
				logg(LOG_ERR, "failed to send result \n");
				close(serv_sock);
				serv_sock = -1;

				continue;
			}

			flush_socket(serv_sock);
		}

		// ========================================================================
		// Whatever you wants
		// ========================================================================
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

int32_t wait_packet(int sock, struct packet_header *packet)
{
	int32_t command = 0;
	int ret;

	if (sock < 0) return -2;

	if ((ret = recv(sock, packet, sizeof(packet), MSG_PEEK | MSG_DONTWAIT)) != sizeof(packet))
		return -(errno != EAGAIN);

	return 1;
}

int32_t handling_command(int sock, struct task_manager *tm)
{
	struct packet_header packet;
	uint8_t *body;
	int ret;

	if (recvt(sock, &packet, PACKET_SIZE, CPS) <= 0)
		return -1;

	if (packet.bsize > 0) {
		body = malloc(sizeof(char) * packet.bsize);
		if (body == NULL)
			return -2;

		if (recvt(sock, body, packet.bsize, CPS) <= 0) {
			free(body); return -3;
		}
	}

	switch (packet.method) {
	case IPC_REGISTER_DEVICE:
		return -6;
		break;

	case IPC_REGISTER_GCODE: {
		if ((ret = register_task(tm, packet.fname, packet.quantity, body, packet.bsize)) < 0) {
			logg(LOG_ERR, "register_task() error %d", ret);
			free(body);
			return -4;
		}
		break;

	case IPC_DELETE_GCODE:
		if (!delete_task(tm, packet.fname)) {
			free(body);
			return -5;
		}
		break;

	case IPC_RENAME_GCODE:
		if (rename_task(tm, packet.fname, packet.rname) < 0)
			return -8;
		break;

	case IPC_CHANGE_QUANTITY_AND_ORDER:
		if (change_task_quantity_and_order(tm, packet.fname, packet.quantity, packet.order) < 0)
			return -7;
		break;

	default:
		return -6;
		break;
	}}

	free(body);

	return 0;
}

int send_packet(int sock, ...)
{
	struct packet_header packet;
	va_list args;

	va_start(args, sock);

	switch ((packet.method = va_arg(args, enum IPC_COMMAND))) {
	case IPC_REGISTER_DEVICE: {
		packet.device_id = va_arg(args, int32_t);
		memcpy(packet.device_key, va_arg(args, uint8_t *), DEVICE_KEY_SIZE);
		break;
	}

	default: break;
	}

	va_end(args);

	if (sendt(sock, &packet, PACKET_SIZE, CPS) <= 0)
		return -1;

	do {
		int32_t result;

		if (recvt(sock, &result, sizeof(result), CPS) <= 0)
			return -2;

		if (result < 0)
			return -3 + result;
	} while (false);

	return 0;
}

