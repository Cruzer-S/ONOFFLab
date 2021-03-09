#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "server_handler.h"
#include "device_manager.h"
#include "http_handler.h"

_Noreturn void error_handling(const char *format, ...);
int client_handling(int sock);
int start_epoll_thread(int epfd, int serv_sock);

int main(int argc, char *argv[])
{
	int serv_sock, epfd;
	int ret;

	if (argc != 3)
		error_handling("usage: <%s> <port> <backlog> \n", argv[0]);

	if ((serv_sock = make_server(strtol(argv[1], NULL, 10),
								 strtol(argv[2], NULL, 10))) < 0)
		error_handling("failed to create server: %d \n", serv_sock);

	if ((ret = change_flag(serv_sock, O_NONBLOCK)) < 0)
		error_handling("change_flag(): %d \n", ret);

	if (change_sockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, true) == -1)
		error_handling("change_sockopt() error \n");

	printf("create server \n");

	epfd = epoll_create1(0);
	if (epfd == -1)
		error_handling("epoll_create() error \n");

	if (register_epoll_fd(epfd, serv_sock, EPOLLIN | EPOLLET) == -1)
		error_handling("register_epoll_fd() error \n");

	printf("start server \n");

	while ((ret = start_epoll_thread(epfd, serv_sock)) < 0)
		fprintf(stderr , "start_epoll_thread() error: %d", ret);

	close(epfd);
	close(serv_sock);

	return 0;
}

int start_epoll_thread(int epfd, int serv_sock)
{
	while (true)
	{
		struct epoll_event *epev = wait_epoll_event(epfd, MAX_EVENT, 1000 * 60);
		if (epev == NULL) return -1;

		if (epev->data.ptr == NULL) break;

		do {
			if (epev->data.fd == serv_sock) {
				int cnt = 0;
				if ((cnt = register_epoll_client(epfd, serv_sock, EPOLLIN | EPOLLET)) < 0) {
					fprintf(stderr, "register_epoll_client() error \n");
					continue;
				}

				printf("accepting %d client(s) \n", cnt);
			} else { // client
				if (epev->events & EPOLLIN) {
					if (recv(epev->data.fd, (char []) { 0x00 }, 1, MSG_PEEK) == 0) {
						printf("disconnect client: %d \n", epev->data.fd);
						delete_epoll_fd(epfd, epev->data.fd);
						continue;
					}

					printf("receive from client: %d \n", epev->data.fd);
					if (client_handling(epev->data.fd) == -1)
						fprintf(stderr, "client_handling(epev->data.fd) error: ");

 				} else if (epev->events & (EPOLLHUP | EPOLLRDHUP)) {
					printf("shutdown client: %d \n", epev->data.fd);
					delete_epoll_fd(epfd, epev->data.fd);
				}
			}
		} while ((*++epev).data.ptr);
	} // while (true)

	return 0;
}

int parse_device_id(char *url)
{
	return 0;
}

int client_handling(int sock)
{
	struct http_header http;
	uint32_t command;
	size_t hsize;
	uint8_t raw_data[HEADER_SIZE], *hp;
	uint8_t *body;

	hsize = 0;
	for (int ret = 0;
		 !((ret == -1) && (errno == EAGAIN)) && hsize < HEADER_SIZE;
		 hsize += (ret = recv(sock, raw_data + hsize, HEADER_SIZE - hsize, 0)))
		raw_data[hsize + ret] = '\0';

	printf("Received header size: %zu \n", hsize);

	if (parse_http_header((char *)raw_data, hsize, &http) < 0) {
		EXTRACT(hp, command);
		switch (command) {
		case IPC_REGISTER_DEVICE: {
			uint32_t dev_id;

			EXTRACT(hp, dev_id);

			if (register_device(sock, dev_id))
				printf("device registered\n"
					   "ID: %d" " | " "socket: %d\n", dev_id, sock);
			else printf("Already registerd: %d \n", dev_id);

			break;
		}

		default: break;
		}
	} else {
		show_http_header(&http);

		int clnt_sock = find_device(strtol(strtok(http.url, "/"), NULL, 10));
		int length = strtol(http.content.length, NULL, 10);
		if (length > 0) {
			int received = hsize - (http.EOH - raw_data);

			printf("After the header: %d \n", received);
			FILE *fp = fopen("receive.dat", "w");
			if (fp == NULL) return -1;

			if (received > length)
				received = length;

			if (fwrite(http.EOH, sizeof(char), received, fp) != received)
				return -2;

			for (int remain = length - received, to_read;
				 received < length;
				 remain = length - received)
			{
				printf("Remaining Data: %d \n", length - received);

				remain = (remain < HEADER_SIZE) ? remain : HEADER_SIZE;
				while ((to_read = recv(sock, raw_data, remain, 0)) == -1) ;

				if (fwrite(raw_data, sizeof(char), to_read, fp) != to_read)
					return -4;

				received += to_read;
			}

			fclose(fp);
		}
	}

	return 0;
}

_Noreturn void error_handling(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	vfprintf(stderr, fmt, ap);

	va_end(ap);

	exit(EXIT_FAILURE);
}
