#ifndef DEBUGGER_H__
#define DEBUGGER_H__

#include <stdarg.h>			// va_list, va_start, va_end
#include <stdio.h>			// vfprintf
#include <stdlib.h>			// exit, EXIT_FAILURE
#include <stdbool.h>		// bool
#include <wiringSerial.h>	// serialDataAvail, serialGetchar
#include <wiringPi.h>		// delayMicroseconds

_Noreturn void error_handling(const char *formatted, ...);
void scrape_serial(int serial, int delay, int maxline, bool inout);

#ifdef DEBUG
#define DPRINT(D, X) printf(#X " = %" #D " \n", X)
#else
#define DPRINTF(D, X) /* empty */
#endif

#endif
