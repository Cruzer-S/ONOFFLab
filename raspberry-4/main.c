#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdalign.h>
#include <threads.h>

#include "servo_hrx.h"

_Noreturn void error_handling(const char *format, ...);

int main(int argc, char *argv[])
{
	struct motor main_motor;
	int main_serial;

	main_motor = hrx_create_motor(0xFD);
	main_serial = open_serial(115200);

	if (main_serial == -1)
		error_handling("failed to open serial\n");

	hrx_connect_serial(&main_motor, main_serial);

	while (true) {
		hrx_set_one_position(main_motor, 255, 100, HRX_LED_RED);
		putchar(serialGetChar(main_motor.serial));
	}
	
	hrx_delete_motor(&main_motor);
	close_serial(main_serial);

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
