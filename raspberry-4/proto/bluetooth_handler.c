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

int simplescan(void)
{
	inquiry_info *ii = NULL;
    int max_rsp, num_rsp;
    int dev_id, sock, len, flags;
    int i;
    char addr[19] = { 0 };
    char name[248] = { 0 };

    dev_id = hci_get_route(NULL);
    sock = hci_open_dev( dev_id );
    if (dev_id < 0 || sock < 0) {
        perror("opening socket");
        exit(1);
    }

    len  = 8;
    max_rsp = 255;
    flags = IREQ_CACHE_FLUSH;
    ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
    
    num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
    if( num_rsp < 0 ) perror("hci_inquiry");

	printf("number of response: %d \n", num_rsp);

    for (i = 0; i < num_rsp; i++) {
        ba2str(&(ii+i)->bdaddr, addr);
        memset(name, 0, sizeof(name));
        if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), 
            name, 0) < 0)
        strcpy(name, "[unknown]");
        printf("%s  %s\n", addr, name);
    }

    free( ii );
    close( sock );
    return 0;
}

int make_bluetooth(int port, int backlog)
{
	struct sockaddr_rc loc_addr;
	int sock;

	sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if (sock == -1)
		return -1;

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
	socklen_t opt = sizeof(clnt_addr);

	memset(&clnt_addr, 0x00, sizeof(clnt_addr));
	client = accept(server, (struct sockaddr *)&clnt_addr, &opt);
	
	return client;
}
