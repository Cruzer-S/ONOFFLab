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

#define SIZEOF(X) (sizeof(X) / sizeof(X[0]))
#define MAXLINE BUFSIZ 
#define WPA_DIRECTORY	"/etc/wpa_supplicant/wpa_supplicant.conf"
#define TEMP_DIRECTORY	"tmp.txt"

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
		error_handling("usage: <%s> <ssid> <passwd>\n", **argv);

	if ((serial_port = serialOpen(SERIAL_PORT_DEVICE, BOAD_RATE)) < 0)
		error_handling("failed to open %s serial: %s \n",
				       SERIAL_PORT_DEVICE, strerror(errno));

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s \n", strerror(errno));

	change_wifi(argv[1], argv[2]);

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

void copy_file(FILE *origin, FILE *dest)/*{{{*/
{
	char buffer[BUFSIZ];

	while (fgets(buffer, BUFSIZ, origin) != NULL)
		fputs(buffer, dest);
}/*}}}*/

bool change_wifi(const char *name, const char *passwd)/*{{{*/
{
	const char *wpa_keywords[] = {
		"ssid", "psk"
	};

	char line[MAXLINE];

	FILE *fp, *tmp = fp = NULL;

	fp = fopen(WPA_DIRECTORY, "r");
	tmp = fopen(TEMP_DIRECTORY, "w");

	if (( fp = fopen( WPA_DIRECTORY, "r")) == NULL
	||  (tmp = fopen(TEMP_DIRECTORY, "w")) == NULL)
		goto FAIL;

	while (readline(line, MAXLINE, fp) > 0)
		for (int i = 0; i < SIZEOF(wpa_keywords); i++) 
			if (!strstr(line, wpa_keywords[i]))
				fprintf(tmp, "%s = %s \n",
						wpa_keywords[i], 
						(i == 0) ? name : passwd);
			else fputs(line, tmp);

	fclose(fp); fclose(tmp);

	fp = tmp = NULL;
	if (( fp = fopen( WPA_DIRECTORY, "w")) == NULL
	||  (tmp = fopen(TEMP_DIRECTORY, "r")) == NULL)
		goto FAIL;
	else
		copy_file(tmp, /* > to > */ fp);

	fclose(fp); fclose(tmp);

	if (!refresh_wifi())
		goto FAIL;

WHEN_IT:; SUCCESS:; return true;
OTHERWISE:;  FAIL:; THEN:;
	if (fp != NULL) fclose(fp);
	if (tmp != NULL) fclose(tmp);

	remove(TEMP_DIRECTORY);
	return false;
}/*}}}*/

bool refresh_wifi(void)/*{{{*/
{
	// alert changing of /etc/wpa_supplicant/wpa_supplicant.conf
	if (system("sudo systemctl daemon-reload"))
		return false;

	// restart dhcpcd which is parent of wpa_supplicant daemon 
	if (system("sudo systemctl restart dhcpcd"))
		return false;

	return true;
}/*}}}*/

_Noreturn void error_handling(const char *fmt, ...)/*{{{*/
{
WHEN_IT: ;
	va_list ap;

	va_start(ap, fmt);

	vfprintf(stderr, fmt, ap);

	va_end(ap);

	exit(EXIT_FAILURE);
}/*}}}*/


