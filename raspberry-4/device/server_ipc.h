#ifndef IPC_MANAGER_H__
#define IPC_MANAGER_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

enum IPC_COMMAND {
	IPC_REGISTER_DEVICE = 0x01,
};

int connect_to_target(const char *host, unsigned short port);
int ipc_to_target(int sock, enum IPC_COMMAND cmd, ...);

#endif
