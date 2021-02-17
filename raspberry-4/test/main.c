#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <wiringPi.h>
#include <wiringSerial.h>

int main (int argc, char *argv[])
{
	int fd;

	if ((fd = serialOpen ("/dev/ttyS0", 115200)) < 0)
	{
		fprintf (stderr, "Unable to open serial device: %s\n", strerror (errno));
		return 1 ;
	}

	printf("fd: %d \n", fd);

	if (wiringPiSetup () == -1)
	{
		fprintf (stdout, "Unable to start wiringPi: %s\n", strerror (errno));
		return 1 ;
	}

	while (true) 
	{
		if (serialDataAvail(fd) > 0)
		{
			printf("<-");
			while (serialDataAvail(fd) > 0)
			{
				printf ("%02x ", serialGetchar (fd));
				delayMicroseconds(51);
			}
			printf("\n");
			fflush (stdout) ;
		}
	}

	return 0 ;
}
