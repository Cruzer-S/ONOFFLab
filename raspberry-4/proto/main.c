#include <stdio.h>
#include <stdlib.h>

#include <wiringPi.h>
#include <wiringSerial.h>

#include "wifi_manager.h"
#include "debugger.h"
#include "server_ipc.h"
#include "bluetooth_handler.h"

#define LINE_PER_BYTE 16
#define DEBUG_DELAY 100
#define BOAD_RATE 115200

#define SERIAL_PORT_DEVICE	"/dev/ttyS0"
#define SERVER_DOMAIN	"www.mythos.ml"

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	short port_num;

	if (argc != 2)
		error_handling("usage: <%s> <port> \n", argv[0]);

	/* =====================================================================
	if ((serial_port = serialOpen(SERIAL_PORT_DEVICE, BOAD_RATE)) < 0)
		error_handling("failed to open %s serial: %s \n",
				       SERIAL_PORT_DEVICE, strerror(errno));
	if (wiringPiSetup() == -1)
		error_handling("unable to start wiringPi: %s \n", strerror(errno));
	if (!change_wifi(argv[1], argv[2]))
		error_handling("failed to change wifi... \n"
					   "ssid: %s \n"	"psk : %s \n",
					   argv[1],			argv[2]);

	serialClose(serial_port);

	===================================================================== */

	if (start_bluetooth() < 0)
		error_handling("start_bluetooth() error");

	if ((serv_sock = make_bluetooth(0, 10)) < 0)
		error_handling("make_bluetooth() error");

	DPRINT(d, serv_sock);

	while (true) {
		if ((clnt_sock = accept_bluetooth(serv_sock)) < 0)
			error_handling("accept_bluetooth() error");

		printf("accept clinet: %d \n", clnt_sock);
	}

	/* =====================================================================
	port_num = (short) strtol(argv[1], NULL, 10);
	printf("port_num: %hd \n", port_num);

	if ((serv_sock = connect_server(SERVER_DOMAIN, port_num)) < 0)
		error_handling("connect_server() error: %d \n", serv_sock);
	
	printf("Connect Successfully !\n");

	while (true) {
		char buffer[BUFSIZ];
		int ret;

		printf("Enter the message: ");
		scanf("%s", buffer);

		if (server_send_data(serv_sock, strlen(buffer) + 1, buffer) == -1)
			error_handling("server_send_data() error \n");

		printf("recv message from server: ");
		ret = server_recv_data(serv_sock, sizeof(buffer), buffer);
		if (ret == -1)
			error_handling("serv_recv_data() error \n");

		fputs(buffer, stdout);

		while (ret > 0) {
			if ((ret = server_recv_data(-1, sizeof(buffer), buffer)) == -1)
				error_handling("serv_recv_data() error \n");

			fputs(buffer, stdout);
		}

		fputc('\n', stdout);
		
		break;
	}

	close(serv_sock);
	===================================================================== */

	return 0;
}
