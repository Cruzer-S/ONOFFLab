#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdalign.h>
#include <threads.h>

#include <wiringPi.h>
#include <wiringSerial.h>

typedef uint8_t byte;

struct alignas(1) packet {
	byte header[2];
	
	byte size;
	byte id;
	byte command;

	byte check[2];
};

_Noreturn void error_handling(const char *format, ...);

int main(void)
{
	int serial;

	int ((serial = serialOpen("/dev/ttyAMA0", 115200)) < 0)
		error_handling("failed to open serial device \n");

	if (wiringPiSetup() == -1)
		error_handling("failed to call wiringPiSetup() \n");

	serialClose(serial);

	return 0;
}

_Noreturn void error_handling(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	vfprintf(stderr, fmt, ap);

	va_end(ap);

	exit(1);
}
