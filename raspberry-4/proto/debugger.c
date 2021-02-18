#include "debugger.h"

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

_Noreturn void error_handling(const char *fmt, ...)/*{{{*/
{
	va_list ap;

	va_start(ap, fmt);

	vfprintf(stderr, fmt, ap);

	va_end(ap);

	exit(EXIT_FAILURE);
}/*}}}*/
