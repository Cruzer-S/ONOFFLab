#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdalign.h>
#include <threads.h>

//#include <wiringPi.h>
//#include <wiringSerial.h>

typedef uint8_t byte;

struct packet {
	byte header[2];
	byte size;
	byte id;
	byte command;
	double x;
	byte check[2];
};

alignas(8) struct packet packet1;
struct packet packet2;

_Noreturn void error_handling(const char *format, ...);

int main(void)
{
	printf("sizeof(packet1): %zu\n", sizeof packet1);
	printf("sizeof(packet2): %zu\n", sizeof packet2);

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
