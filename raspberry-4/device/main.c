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
#include "server_ipc.h"

#define BOAD_RATE			9600
#define SERIAL_PORT_DEVICE	"/dev/ttyS0"

#define SERVER_DOMAIN		"localhost"
#define SERVER_PORT			1584

#define SERVER_SYNC_TIME	(CLOCKS_PER_SEC * 5)

#define DEVICE_ID			0x00000001

bool is_initiate(int serial);
int parse_data(int serial, char *ssid, char *psk);

int main(int argc, char *argv[])
{
	int serial_port, serv_sock;
	unsigned short port_num;

	if (argc != 2)
		error_handling("usage: %s <port> \n", argv[0]);

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s \n", strerror(errno));

	if ((serial_port = serialOpen(SERIAL_PORT_DEVICE, BOAD_RATE)) < 0)
		error_handling("failed to open %s serial: %s \n",
				       SERIAL_PORT_DEVICE, strerror(errno));

	do {
		long test = strtol(argv[1], NULL, 10);
		if (test < 0 || test > USHRT_MAX)
			error_handling("port number(%ld) out of range \n", test);

		port_num = (unsigned short) test;
	} while (false);

	while (true)
	{
		// ========================================================================
		// checking whether the serial data is available.
		// ========================================================================
		if (serialDataAvail(serial_port)) {
			if (is_initiate(serial_port)) {
				fprintf(stderr, "initiate comes from bluetooth \n");

				char ssid[SSID_SIZ + 1];
				char psk[PSK_SIZ + 1];

				if (parse_data(serial_port, ssid, psk)) {
					fprintf(stderr, "parse success! (%s, %s) \n", ssid, psk);
					if (!change_wifi(ssid, psk)) {
						fprintf(stderr, "failed to change wifi \n");
						serialPutchar(serial_port, 0x00);
					} else {
						serialPutchar(serial_port, 0x01);
					}
				} else { // if (parse_data(serial_port, ssid, psk))
					fprintf(stderr, "parse_data(serial_port, ssid, psk) error! \n");
				}
			} else { // if (is_initiate(serial_port))
				fprintf(stderr, "is_initiate(serial_port) error! \n");
			}
		} // if (serialDataAvail(serial_port))

		// ========================================================================
		//
		// ========================================================================
		if ((serv_sock = connect_server(SERVER_DOMAIN, port_num)) < 0)
			error_handling("connect_server() error: %d \n", serv_sock);
	}

	close(serv_sock);
	serialClose(serial_port);

	while (true) {
		char buffer[BUFSIZ];
		int ret;

		printf("Enter the message: ");
		scanf("%s", buffer);

		if (server_send_data(serv_sock, strlen(buffer) + 1, buffer) == -1)
			error_handling("server_send_data() error \n");

		printf("recv message from server: ");
		ret = server_recv_data(serv_sock, sizeof(buffer), buffer);
		if (ret == -1)
			error_handling("serv_recv_data() error \n");

		fputs(buffer, stdout);

		while (ret > 0) {
			if ((ret = server_recv_data(-1, sizeof(buffer), buffer)) == -1)
				error_handling("serv_recv_data() error \n");

			fputs(buffer, stdout);
		}

		fputc('\n', stdout);

		break;
	}

	return 0;
}

#define INITIATE_TIMEOUT		((clock_t) CLOCKS_PER_SEC)
#define PROTOCOL_KEY_SIZE		((size_t) sizeof(BLUETOOTH_PROTOCOL_KEY) * CHAR_BIT)

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
		printf("%02X ", ch);

		switch (step) {
		case 0: ssid_size = ch;
				step++;
				fprintf(stderr, "ssid size: %u \n", ssid_size);
				break;

		case 1: psk_size = ch;
				step++;
				fprintf(stderr, "psk size: %u \n", psk_size);
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
