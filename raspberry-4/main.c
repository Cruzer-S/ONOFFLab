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

	hrx_connect_serial(&main_motor, main_serial);

	while (true)
		hrx_set_one_position(main_motor, 255, 100, HRX_LED_RED);
	
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

void hrx_calc_checksum(struct packet *pk, int n, byte data[n])
{
	byte result = pk->size;
	
	result ^= pk->id;
	result ^= pk->command;
	
	for (int i = 0; i < n; i++)
		result ^= data[i];

	result &= 0xFE;

	pk->check[0] = result;
	pk->check[1] = (~result) & 0xFE;
}

void hrx_set_one_position(struct motor motor, int position, unsigned time, enum color color)
{
	struct {
		//JOG (LSB + MSB)
		unsigned data		:14;
		unsigned inf		: 1;
		unsigned reserved1	: 1;

		// SET
		unsigned stop		: 1;
		unsigned mode		: 1;
		unsigned led		: 3;
		unsigned invalid	: 1;
		unsigned reserved2	: 2;

		// ID
		unsigned id			: 8;

		// playtime
		unsigned time		: 8;
	} IJOG_TAG = {
		.data = position,	// set position
		.inf = (position < 0) ? 1 : 0,
							// infinite

		.stop = 0,			// Don't stop
		.mode = 0,			// position mode
		.led  = color,		// set color
		.invalid =  0,		// when invalid is true,
							// it means no action.
		.id = motor.id,		// Motor ID

		.time = time		// Playtime
	};

	hrx_send_data(motor, HRX_CMD_I_JOG, sizeof(IJOG_TAG), (byte *)&IJOG_TAG);
}

bool hrx_send_data(struct motor motor, enum command command, int n, byte data[n])
{
	struct packet packet;

	packet = (struct packet) {
		.header = { 0xFF, 0xFF },
		.size = sizeof(struct packet) + n,
		.id = motor.id,
		.command = command
	};

	hrx_calc_checksum(&packet, n, data);

	for (int i = 0; i < sizeof(struct packet); i++)
		serialPutchar(motor.serial, *((char *)&packet + i));
	
	for (int i = 0; i < n; i++)
		serialPutchar(motor.serial, data[i]);

	return true;
}

void hrx_delete_motor(struct motor *motor)
{
	motor->id = -1;
	motor->serial = -1;
}

int open_serial(int baud)
{
	int serial = serialOpen("/dev/ttyAMA0", baud);

	return (serial < 0) ? -1 : serial;
}

void close_serial(int serial)
{
	serialClose(serial);
}

struct motor hrx_connect_serial(struct motor *motor, int serial)
{
	motor->serial = serial;	
}
