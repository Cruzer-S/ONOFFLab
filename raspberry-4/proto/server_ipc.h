#ifndef SERVER_IPC_H__
#define SERVER_IPC_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <arpa/inet.h>

int connect_server(const char *host, short port);
int send_server(int serv_sock, uint32_t size, char *data);

#endif
