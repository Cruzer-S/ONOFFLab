#ifndef BLUETOOTH_HANDLER_H__
#define BLUETOOTH_HANDLER_H__

#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

int start_bluetooth(void);
int make_bluetooth(int port, int backlog);
int accept_bluetooth(int sock);

#endif
