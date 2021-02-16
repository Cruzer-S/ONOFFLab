#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdalign.h>
#include <threads.h>

#include <wiringPi.h>
#include <wiringSerial.h>

#define BROADCAST_ID 0xFE

#define DECLARE_ENUM_COMMAND(PREFIX)\
	enum command {					\
		PREFIX##EEP_WRITE = 0x01,	\
		PREFIX##EEP_READ,			\
		PREFIX##RAM_READ,			\
		PREFIX##RAM_WRITE,			\
		PREFIX##I_JOG,				\
		PREFIX##S_JOG,				\
		PREFIX##STAT,				\
		PREFIX##ROLLBACK,			\
		PREFIX##REBOOT				\
	};

DECLARE_ENUM_COMMAND(CMD_)

typedef uint8_t byte;

struct packet {
	byte header[2];
	byte size;
	byte id;
	byte command;
	byte check[2];
};

_Noreturn void error_handling(const char *format, ...);

int main(int argc, char *argv[])
{
	printf("%d", CMD_EEP_WRITE);

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
