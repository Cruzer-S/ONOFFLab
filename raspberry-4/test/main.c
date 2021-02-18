#include <stdio.h>		// standard I/O

#include <wiringPi.h>
#include <wiringSerial.h>

#include "wifi_manager.h"
#include "debugger.h"

#define LINE_PER_BYTE 16
#define DEBUG_DELAY 100
#define BOAD_RATE 115200
#define SERIAL_PORT_DEVICE	"/dev/ttyS0"

int main(int argc, char *argv[])
{
	int serial_port;

	if (argc != 3)
		error_handling("usage: <%s> <ssid> <psk>\n", argv[0]);

	if ((serial_port = serialOpen(SERIAL_PORT_DEVICE, BOAD_RATE)) < 0)
		error_handling("failed to open %s serial: %s \n",
				       SERIAL_PORT_DEVICE, strerror(errno));

	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s \n", strerror(errno));

	if (!change_wifi(argv[1], argv[2]))
		error_handling("failed to change wifi... \n"
					   "ssid: %s \n"	"psk : %s \n",
					   argv[1],			argv[2]);

	printf("test auto commit");

	serialClose(serial_port);

	return 0;
}
