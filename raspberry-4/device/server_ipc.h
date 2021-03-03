#ifndef SERVER_IPC_H__
#define SERVER_IPC_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

enum SERVER_IPC_COMMAND {
	SIC_REG = 0x01, SIC_REQ, SIC_SYN
};

int connect_to_server(const char *host, short port);
int command_to_server(enum SERVER_IPC_COMMAND cmd, ...);
int server_send_data(int serv_sock, uint32_t size, char *data);
int server_recv_data(int serv_sock, uint32_t max, char *data);

#endif
