#include <stdio.h>
#include <stdlib.h>

#include <wiringPi.h>
#include <wiringSerial.h>

#include "wifi_manager.h"
#include "debugger.h"
#include "server_ipc.h"

#define LINE_PER_BYTE 16
#define DEBUG_DELAY 100
#define BOAD_RATE 115200

#define SERIAL_PORT_DEVICE	"/dev/ttyS0"

#define SERVER_DOMAIN	"www.mythos.ml"

int main(int argc, char *argv[])
{
	int serial_port, serv_sock;

	if (argc != 2)
		error_handling("usage: <%s> <port> \n", argv[0]);

	printf("strtol(argv[1]): %ld \n", strtol(argv[1], NULL, 10));

	if ((serv_sock = connect_server(SERVER_DOMAIN, strtol(argv[1], NULL, 10))) < 0)
		error_handling("connect_server() error: %d \n", serv_sock);
	
	printf("Connect Successfully !\n");

	while (true) {
		char buffer[BUFSIZ];

		printf("Enter the message: ");
		scanf("%s", buffer);

		if (server_send_data(serv_sock, strlen(buffer) + 1, buffer) == -1)
			error_handling("server_send_data() error");

		for (int ret = server_recv_data(serv_sock, sizeof(buffer), buffer);
			 ret == -1 ? error_handling("server_recv_data() error"), 0 : 
			 ret  >  0 ? true : false, 0;
			 ret -= server_recv_data(-1, sizeof(buffer), buffer))
			fputs(buffer, stdout);
	}
	
	/*
	serialClose(serial_port);
	*/

	return 0;
}
