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
#include "ipc_mananger.h"

#define BOAD_RATE			9600
#define SERIAL_DEVICE		"/dev/ttyS0"

#define SERVER_DOMAIN		"www.mythos.ml"
#define SERVER_PORT			1584

#define SERVER_SYNC_TIME	(CLOCKS_PER_SEC * 10)

#define DEVICE_ID			0x00000001

bool is_initiate(int serial);
int parse_data(int serial, char *ssid, char *psk);

struct timestp {
	uint8_t year:6;		// 2020 + (1 ~ 64)
	uint8_t month:4;	// 1 ~ 12 (1 ~ 16)
	uint8_t day:5;		// 1 ~ 31 (1 ~ 32)

	uint8_t hour:5;		// 0 ~ 23 (0 ~ 31)
	uint8_t minute:6;	// 0 ~ 59 (0 ~ 63)
	uint8_t second:6;	// 0 ~ 59 (0 ~ 63)
} __attribute__((packed));

struct device_state {
	uint8_t state:3;
	uint8_t progs:5;

	struct timestp start;
	struct timestp predict;
} __attribute__((packed));

struct header {
} __attribute__((packed));

int main(int argc, char *argv[])
{
	int bluetooth_port, serv_sock;
	unsigned short port_num;
	struct device_state dev_stat;

	if (argc != 2)
		error_handling("usage: %s <port>", argv[0]);

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s", strerror(errno));

	if ((bluetooth_port = serialOpen(SERIAL_DEVICE, BOAD_RATE)) < 0)
		error_handling("failed to open %s serial: %s",
				       SERIAL_DEVICE, strerror(errno));

	do { // check port number range
		long test;

		test = strtol(argv[1], NULL, 10);
		if (test < 0 || test > USHRT_MAX)
			error_handling("port number(%ld) out of range", test);

		port_num = (unsigned short) test;

		if ((serv_sock = connect_to_target(SERVER_DOMAIN, port_num)) < 0)
			error_handling("connect_to_target() error: %d", serv_sock);
	} while (false);

	if (change_flag(serv_sock, O_NONBLOCK) < 0)
		error_handling("change_flag() error");

	if (ipc_to_target(serv_sock, IPC_REGISTER_DEVICE, DEVICE_ID) < 0)
		error_handling("ipc_to_target() error");

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
		}
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
