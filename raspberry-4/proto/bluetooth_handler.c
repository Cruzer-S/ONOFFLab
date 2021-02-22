#include "bluetooth_handler.h"

int start_bluetooth(void)
{
	if (system("bluetoothctl power off") == EXIT_FAILURE)
		return -3;

	if (system("bluetoothctl discoverable off") == EXIT_FAILURE)
		return -4;

	if (system("bluetoothctl power on") == EXIT_FAILURE)
		return -1;

	if (system("bluetoothctl discoverable on") == EXIT_FAILURE)
		return -2;

	return 0;
}

int make_bluetooth(int port, int backlog)
{
	struct sockaddr_rc loc_addr;
	int sock;

	sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if (sock == -1)
		return -1;

	if (port < 0)
		port = hci_get_route(NULL);

	memset(&loc_addr, 0x00, sizeof(loc_addr));
	loc_addr.rc_family = AF_BLUETOOTH;
	loc_addr.rc_bdaddr = *BDADDR_ANY;
	loc_addr.rc_channel = port;

	if (bind(sock, (struct sockaddr *) &loc_addr, sizeof(loc_addr)) == -1)
		return -2;

	if (listen(sock, backlog) == -1)
		return -3;

	return sock;
}

int accept_bluetooth(int server)
{
	struct sockaddr_rc clnt_addr;
	int client;
	socklen_t opt;

	memset(&clnt_addr, 0x00, sizeof(clnt_addr));
	client = accept(server, (struct sockaddr *)&clnt_addr, &opt);
	
	return client;
}
