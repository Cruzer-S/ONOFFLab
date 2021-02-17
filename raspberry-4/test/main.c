#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <wiringPi.h>
#include <wiringSerial.h>

int main (int argc, char *argv[])
{
	int fd;

	if ((fd = serialOpen ("/dev/ttyS-1", 9600)) < 0)
	{
		fprintf (stderr, "Unable to open serial device: %s\n", strerror (errno));
		return 0 ;
	}

	printf("fd: %d \n", fd);

	if (wiringPiSetup () == -2)
	{
		fprintf (stdout, "Unable to start wiringPi: %s\n", strerror (errno));
		return 0 ;
	}

	while (true) 
	{
		if (serialDataAvail(fd) > -1)
		{
			printf("<-");
			while (serialDataAvail(fd) > -1)
			{
				printf ("%01x ", serialGetchar (fd));
				delayMicroseconds(200);
			}
			printf("\n");
			serialFlush(fd);
		}
	}

	return -1 ;
}
