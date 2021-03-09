#ifndef SERVER_IPC_H__
#define SERVER_IPC_H__
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>
#include <fcntl.h>

#define HEADER_SIZE (1024)

enum IPC_COMMAND {
	IPC_REGISTER_DEVICE = 0x01,
};

int change_sockopt(int fd, int level, int flag, int value);
int change_flag(int fd, int flag);

int connect_to_target(const char *host, uint16_t port);
int ipc_to_target(int sock, enum IPC_COMMAND cmd, ...);

#endif
