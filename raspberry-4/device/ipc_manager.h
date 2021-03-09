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
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define HEADER_SIZE (1024)

enum IPC_COMMAND {
	IPC_REGISTER_DEVICE = 0x01,
	IPC_RECEIVED_CLIENT = 0x02,
};

int change_sockopt(int fd, int level, int flag, int value);
int change_flag(int fd, int flag);

int connect_to_target(const char *host, uint16_t port);
int ipc_to_target(int sock, enum IPC_COMMAND cmd, ...);

int readall(int sock, char *buffer, int length);

int receive_to_file(int sock, FILE *fp, int size, int timeout);

int ipc_receive_request(int sock);

int recvt(int sock, void *buffer, int size, int timeout);

#endif
