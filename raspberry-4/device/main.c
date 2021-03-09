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

#define BOAD_RATE			9600
#define SERIAL_DEVICE		"/dev/ttyS0"

#define SERVER_DOMAIN		"www.mythos.ml"
#define SERVER_PORT			1584

#define SERVER_SYNC_TIME	(CLOCKS_PER_SEC * 10)

#define DEVICE_ID			0x00000001

bool is_initiate(int serial);
int parse_data(int serial, char *ssid, char *psk);

int main(int argc, char *argv[])
{
	int bluetooth_port, serv_sock;

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s", strerror(errno));

	printf("setup wiring pi \n");

	if ((bluetooth_port = serialOpen(SERIAL_DEVICE, BOAD_RATE)) < 0)
		error_handling("failed to open %s serial: %s",
				       SERIAL_DEVICE, strerror(errno));

	printf("open bluetooth port \n");

	do {
		long check;
		const char *host;
		uint16_t port;

		check = (argc == 3) ? strtol(argv[2], NULL, 10) : SERVER_PORT;
		if (check < 0 || check > USHRT_MAX)
			error_handling("port number out of range", check);

		port = (uint16_t) check;
		host = (argc == 3) ? argv[1] : SERVER_DOMAIN;

		if ((serv_sock = connect_to_target(host, port)) < 0)
			error_handling("connect_to_target(%s, %hd) error", host, port);

		printf("connect to server: %d\n"
			   "Address: %s:%hd\n",
				serv_sock, host, port);
	} while (false);

	/*
	if (change_flag(serv_sock, O_NONBLOCK) < 0)
		error_handling("change_flag() error");
	*/

	if (ipc_to_target(serv_sock, IPC_REGISTER_DEVICE, DEVICE_ID) < 0)
		error_handling("ipc_to_target() error");

	printf("register device: %d \n", DEVICE_ID);

	for (clock_t start, end = start = clock(); ; end = clock()) {
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

			printf("change wifi ssid and psk\nssid: %s\tpsk: %s\n",
					ssid, psk);
		}

		uint32_t command;
		if (recvt(serv_sock, &command, sizeof(command), 1000) < 0)
			continue;

		printf("Command: %d \n", command);
		switch (command) {
		case IPC_REGISTER_DEVICE: break;
		case IPC_RECEIVED_CLIENT: {
			uint32_t length;

			if (recvt(serv_sock, &length, sizeof(length), 1000) < 0)
				break;

			printf("Length: %u \n", length);

			FILE *fp = fopen("test.dat", "w");
			if (fp == NULL) break;

			if (receive_to_file(serv_sock, fp, length, 1000) < 0) {
				fclose(fp);
				break;
			}

			fclose(fp);

			printf("Received successfully \n");
		}}
	}

	close(serv_sock);
	serialClose(bluetooth_port);

	return 0;
}

#define INITIATE_TIMEOUT		((clock_t) CLOCKS_PER_SEC)

int parse_data(int serial, char *ssid, char *psk)
{
	int step = 0, ch;
	uint8_t ssid_size, psk_size;

	for (clock_t end, start = end = clock();
		 (end - start) < INITIATE_TIMEOUT;
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
		 (end - start) < INITIATE_TIMEOUT;
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
