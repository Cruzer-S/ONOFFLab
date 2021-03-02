#ifndef SERVER_IPC_H__
#define SERVER_IPC_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

int connect_server(const char *host, short port);
int server_send_data(int serv_sock, uint32_t size, char *data);
int server_recv_data(int serv_sock, uint32_t max, char *data);

#endif
