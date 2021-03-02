#include "debugger.h"

#define LINE_PER_BYTE 16
#define DEBUG_DELAY 100

#define DEBUG_BUFFER 1024

void debug_serial(int serial, bool inout)
{
	if (serialDataAvail(serial) > 0) {
		fputs("\n    ", stdout);
		for (int i = 0; i < LINE_PER_BYTE; i++)
			printf("0x%02X ", i);
		fputc('\n', stdout);

		fputs(inout ? "<-- " : ">-- ", stdout);
		for (int i = 1, k = 1; serialDataAvail(serial) > 0; i++) {
			printf("  %02X ", serialGetchar(serial));
			delayMicroseconds(DEBUG_DELAY);

			if (i == LINE_PER_BYTE) {
				printf("\n0x%X ", k++);
				i = 0;
			}
		}

		fputc('\n', stdout);
	}
}

_Noreturn void error_handling(const char *fmt, ...)
{
	va_list ap;
	char message[DEBUG_BUFFER];

	va_start(ap, fmt);

	vsprintf(message, fmt, ap);

	va_end(ap);

	fprintf(stderr, "%s: %s\n", message, strerror(errno));

	exit(EXIT_FAILURE);
}
