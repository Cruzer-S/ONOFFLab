#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <errno.h>

#include <wiringPi.h>
#include <wiringSerial.h>

#include "wifi_manager.h"
#include "debugger.h"
#include "server_ipc.h"

#define BOAD_RATE			9600
#define SERIAL_PORT_DEVICE	"/dev/ttyS0"

#define SERVER_DOMAIN	"www.mythos.ml"
#define SERVER_PORT		1584

bool is_initiate(int serial);
int parse_data(int serial, char *ssid, char *psk);

int main(int argc, char *argv[])
{
	int serial_port;

	if ((serial_port = serialOpen(SERIAL_PORT_DEVICE, BOAD_RATE)) < 0)
		error_handling("failed to open %s serial: %s \n",
				       SERIAL_PORT_DEVICE, strerror(errno));

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s \n", strerror(errno));

	while (true) {
		if (serialDataAvail(serial_port)) {
			if (is_initiate(serial_port)) {
				fprintf(stderr, "initiate comes from bluetooth \n");

				char ssid[SSID_SIZ + 1];
				char psk[PSK_SIZ + 1];

				if (parse_data(serial_port, ssid, psk)) {
					if (!change_wifi(ssid, psk))
						error_handling("change_wifi(%s, %s) \n", ssid, psk);
				} else fprintf(stderr, "parse_data(serial_port, ssid, psk) error! \n");
			} else fprintf(stderr, "is_initiate(serial_port) error! \n");
		}
	}

	serialClose(serial_port);

	/* =====================================================================
	port_num = (short) strtol(argv[1], NULL, 10);
	printf("port_num: %hd \n", port_num);

	if ((serv_sock = connect_server(SERVER_DOMAIN, port_num)) < 0)
		error_handling("connect_server() error: %d \n", serv_sock);

	printf("Connect Successfully !\n");

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

	close(serv_sock);
	===================================================================== */

	return 0;
}

#define INITIATE_TIMEOUT		((clock_t) CLOCKS_PER_SEC * 1024)
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
				break;

		case 1: psk_size = ch;
				step++;
				break;

		case 2: if (ssid_size-- > 0) {
					*ssid = '\0';
					step++;
					continue;
				} else *ssid++ = ch;
				break;

		case 3: if (psk_size-- > 0) {
					*psk = '\0';
					step++;
					continue;
				} else *psk++ = ch;
				break;

		case 4: return 1;
		}
	}

	return -1;
}

bool is_initiate(int serial)
{
	static const uint8_t key[] = { 0x12, 0x34, 0x56, 0x78, (uint8_t) '\0'};
	const uint8_t *ptr = key;
	int ch;

	for (clock_t end, start = end = clock();
		 (end - start) < INITIATE_TIMEOUT;
		 end = clock())
	{
		if (serialDataAvail(serial) == -1)
			continue;

		ch = serialGetchar(serial);
		printf("%02X vs. %02X\n", ch, *ptr);
		if (ch != *ptr++)
			return false;

		if (!*ptr) return true;
	}

	return false;
}
