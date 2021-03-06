#ifndef DEBUGGER_H__
#define DEBUGGER_H__

#include <stdarg.h>			// va_list, va_start, va_end
#include <stdio.h>			// vfprintf
#include <stdlib.h>			// exit, EXIT_FAILURE
#include <stdbool.h>		// bool
#include <errno.h>			// errno
#include <string.h>			// strerror

#include <wiringSerial.h>	// serialDataAvail, serialGetchar
#include <wiringPi.h>		// delayMicroseconds

_Noreturn void error_handling(const char *formatted, ...);
void scrape_serial(int serial, bool inout);

#ifdef DEBUG
	#define DSHOW(D, X) fprintf(stderr, #X " = %" #D " \n", X)
	#define DPRINT(FMT, ...) fprintf(stderr, FMT "\n", __VA_ARGS__)
#else
	#define DSHOW(D, X) /* empty */
	#define DPRINT(FMT, ...) /* empty */
#endif

#endif
