#include <stdio.h>
#include <stdlib.h>	// exit(), EXIT_FAILURE
#include <stdint.h> //uint32_t
#include <stdbool.h>
#include <stdarg.h> //variable argument
#include <string.h> //strerror
#include <errno.h> //errno

#include <sys/socket.h>

#include <wiringPi.h>
#include <wiringSerial.h>

#include "wifi_manager.h"

#define LINE_PER_BYTE 16
#define DEBUG_DELAY 100
#define BOAD_RATE 115200
#define SERIAL_PORT_DEVICE	"/dev/ttyS0"

#define SIZEOF(X) (sizeof(X) / sizeof(X[0]))
#define MAXLINE BUFSIZ 
#define WPA_DIRECTORY "/etc/wpa_supplicant/wpa_supplicant.conf"

_Noreturn void error_handling(const char *formatted, ...);

void scrape_serial(int serial, int delay, int maxline, bool inout);
int readline(char *line, int maxline, FILE *fp);

void change_wifi(const char *name, const char *passwd);

int main(int argc, char *argv[])
{
	int serial_port;
	int serv_sock, clnt_sock;

	if ((serial_port = serialOpen(SERIAL_PORT_DEVICE, BOAD_RATE)) < 0)
		error_handling("failed to open %s serial: %s \n", 
				       SERIAL_PORT_DEVICE, strerror(errno));

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s \n", strerror(errno));

	while (true)
	{
		serialPuts(serial_port, 
		"hello, world! my name is yeounsu moon good to see you :)");

		delay(30);
		scrape_serial(serial_port, DEBUG_DELAY, LINE_PER_BYTE, true);

		delay(2000);
	}

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

int readline(char *line, int maxline, FILE *fp)/*{{{*/
{
	const char *origin = line;
	int ch;

	while ((ch = fgetc(fp)) != EOF
		&& (line - origin) < maxline) 
	{
		*line++ = ch;
		if (ch == '\n') break;
	}

	*line = '\0';

	return line - origin;
}/*}}}*/

bool change_wifi(const char *name, const char *passwd)/*{{{*/
{
	const char *wpa_keywords[] = {
		"ssid", "psk"
	};

	char line[MAXLINE];

	FILE *fp = fopen(WPA_DIRECTORY, "r");
	FILE *tmp = fopen("tmp.txt", "w");

	if (fp == NULL)
		return false;

	while (readline(line, MAXLINE, fp) > 0)
	{
		for (int i = 0; i < SIZEOF(wpa_keywords); i++) {
			if (!strstr(line, wpa_keywords[i])) {
				fputs(wpa_keywords[i], tmp);
				fputs(" = \"", tmp);
				fputs(name, tmp);
				fputs("\"\n", tmp);
			} else {
				fputs(line, tmp);
			}
		}
	}

	fclose(tmp);
	fclose(fp);

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
