#include <stdio.h>
#include <stdlib.h>	// exit(), EXIT_FAILURE
#include <stdint.h> //uint32_t
#include <stdbool.h>
#include <stdarg.h> //variable argument
#include <string.h> //strerror
#include <errno.h> //errno

#include <wiringPi.h>
#include <wiringSerial.h>

#define SERIAL_PORT_DEVICE	"/dev/ttyS0"

_Noreturn void error_handling(const char *formatted, ...);

int main(void)
{
	int serial_port;

	if ((serial_port = serialOpen(SERIAL_PORT_DEVICE, 115200)) < 0)
		error_handling("failed to open %s serial: %s \n", SERIAL_PORT_DEVICE, strerror(errno));

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s \n", strerror(errno));

	while (true)
	{
		if (serialDataAvail(serial_port) > 0) {
			char ch;

			fputs("<--", stdout);
			
			while (serialDataAvail(serial_port) > 0) {
				printf("%04x ", (ch = serialGetchar(serial_port)));
				delayMicroseconds(200);
			}

			if (ch == '!')
				fputs("==========================================\n", stdout);
		}

	}

	serialClose(serial_port);


	return 0;
}

_Noreturn void error_handling(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	vfprintf(stderr, fmt, ap);

	va_end(ap);

	exit(EXIT_FAILURE);
}
