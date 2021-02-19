#ifndef BLUETOOTH_HANDLER_H__
#define BLUETOOTH_HANDLER_H__

#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#define BLUETOOTH_PORT_DEVICE "/dev/rfcomm0"

static int start_bluetooth(void)
{
	if (system("bluetoothctl power on") == EXIT_FAILURE)
		return -1;
	if (system("bluetoothctl discoverable on") == EXIT_FAILURE)
		return -2;

	return 0;
}

int make_bluetooth(void)
{
	inquiry_info *ii = NULL;
	int max_rsp, num_rsp;
	int dev_id, sock, len, flags;
	int i;

	char addr[19] = { 0 };
	char name[248] = { 0 };

	dev_id = hci
}

#endif
