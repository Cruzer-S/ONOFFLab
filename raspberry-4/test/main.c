#include <stdio.h>		// standard I/O
#include <stdlib.h>		// exit(), EXIT_FAILURE
#include <stdint.h>		// uint32_t
#include <stdbool.h>	// true, false, bool
#include <stdarg.h>		// variable argument
#include <string.h>		// strerror
#include <errno.h>		// errno

#include <sys/socket.h>	// socket

#include <wiringPi.h>
#include <wiringSerial.h>

#include "wifi_manager.h"
#include "json_parser.h"

#define LINE_PER_BYTE 16
#define DEBUG_DELAY 100
#define BOAD_RATE 115200
#define SERIAL_PORT_DEVICE	"/dev/ttyS0"

#define WPA_DIRECTORY	"/etc/wpa_supplicant/wpa_supplicant.conf"

struct packet {
	uint32_t size;
	void *ptr;
};

_Noreturn void error_handling(const char *formatted, ...);

void scrape_serial(int serial, int delay, int maxline, bool inout);

bool change_wifi(const char *name, const char *passwd);
bool refresh_wifi(void);
int readline(char *line, int maxline, FILE *fp);
void copy_file(FILE *origin, FILE *dest);

void receive_data(struct packet packet);
void send_data(struct packet packet);

int main(int argc, char *argv[])
{
	int serial_port;

	if (argc != 3)
		error_handling("usage: <%s> <ssid> <passwd>\n", argv[0]);

	if ((serial_port = serialOpen(SERIAL_PORT_DEVICE, BOAD_RATE)) < 0)
		error_handling("failed to open %s serial: %s \n",
				       SERIAL_PORT_DEVICE, strerror(errno));

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s \n", strerror(errno));

	if (!change_wifi(argv[1], argv[2]))
		error_handling("failed to change wifi... \n"
					   "ssid: %s \n"	"psk : %s \n",
					   argv[1],			argv[2]);

	serialClose(serial_port);

	return 0;
}

void scrape_serial(int serial, int delay, int maxline, bool inout)/*{{{*/
{	
	if (serialDataAvail(serial) > 0) {
		fputs("\n    ", stdout);
		for (int i = 0; i < maxline; i++)
			printf("0x%02X ", i);
		fputc('\n', stdout);

		fputs(inout ? "<-- " : ">-- ", stdout);
		for (int i = 1, k = 1; serialDataAvail(serial) > 0; i++) {
			printf("  %02X ", serialGetchar(serial));
			delayMicroseconds(delay);

			if (i == maxline) {
				printf("\n0x%X ", k++);
				i = 0;
			}
		}

		fputc('\n', stdout);
	}
}/*}}}*/

bool change_wifi(const char *name, const char *passwd)/*{{{*/
{
	FILE *fp;
	const char *form = {
		"country = US\n"
		"ctrl_interface = DIR=/var/run/wpa_supplicant GROUP=netdev\n"
		"update_config = 1\n"
		"\n"
		"network = {\n"
		"\tssid = \"%s\"\n"
		"\tscan_ssid = 1\n"
		"\tpsk = \"%s\"\n"
		"\tkey_mgmt = WPA-PSK\n"
		"}"
	};

	fp = fopen(WPA_DIRECTORY, "w");
	if (fp == NULL)
		return false;

	fprintf(fp, form, name, passwd);

	fclose(fp);

	if (!refresh_wifi())
		return false;

	return true;
}/*}}}*/

bool refresh_wifi(void)/*{{{*/
{
	// alert changing of /etc/wpa_supplicant/wpa_supplicant.conf
	if (system("sudo systemctl daemon-reload") == EXIT_FAILURE)
		return false;

	// restart dhcpcd which is parent of wpa_supplicant daemon 
	if (system("sudo systemctl restart dhcpcd") == EXIT_FAILURE)
		return false;

	return true;
}/*}}}*/

_Noreturn void error_handling(const char *fmt, ...)/*{{{*/
{
	va_list ap;

	va_start(ap, fmt);

	vfprintf(stderr, fmt, ap);

	va_end(ap);

	exit(EXIT_FAILURE);
}/*}}}*/


