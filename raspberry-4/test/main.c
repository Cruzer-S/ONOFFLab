#include <stdio.h>
#include <stdlib.h>	// exit(), EXIT_FAILURE
#include <stdint.h> //uint32_t
#include <stdbool.h>
#include <stdarg.h> //variable argument
#include <string.h> //strerror
#include <errno.h> //errno

#include <wiringPi.h>
#include <wiringSerial.h>

#define LINE_PER_BYTE 20
#define DEBUG_DELAY 100
#define SERIAL_PORT_DEVICE	"/dev/ttyS0"

_Noreturn void error_handling(const char *formatted, ...);

void scrape_input_serial(int serial, int delay, int maxline);

int main(void)
{
	int serial_port;

	if ((serial_port = serialOpen(SERIAL_PORT_DEVICE, 115200)) < 0)
		error_handling("failed to open %s serial: %s \n", 
				       SERIAL_PORT_DEVICE, strerror(errno));

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s \n", strerror(errno));

	while (true)
	{
		scrape_input_serial(serial_port, DEBUG_DELAY, LINE_PER_BYTE);
	}

	serialClose(serial_port);

	return 0;
}

void scrape_input_serial(int serial, int delay, int maxline)
{
	if (serialDataAvail(serial) > 0) {
		fputs("<-- ", stdout);
		for (int i = 1; serialDataAvail(serial) > 0; i++) {	
			printf("%02X ", serialGetchar(serial));
			delayMicroseconds(100);

			if (i == maxline) {
				fputs("\n    ", stdout);
				i = 0;
			}
		}

		fputc('\n', stdout);
	}
}

_Noreturn void error_handling(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	vfprintf(stderr, fmt, ap);

	va_end(ap);

	exit(EXIT_FAILURE);
}
