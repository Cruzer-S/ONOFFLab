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
				if ((cnt = accept_epoll_client(epfd, serv_sock, EPOLLIN | EPOLLET)) < 0) {
					fprintf(stderr, "accept_epoll_client() error \n");
					continue;
				}

				printf("accepting %d client(s) \n", cnt);
			} else { // client
				int ret;
				if (epev->events & EPOLLIN) {
					if (recv(epev->data.fd, (char []) { 0x00 }, 1, MSG_PEEK) == 0) {
						printf("disconnect client: %d \n", epev->data.fd);
						delete_epoll_fd(epfd, epev->data.fd);
						continue;
					}

					printf("receive data from client: %d \n", epev->data.fd);
					if ((ret = client_handling(epev->data.fd)) < 0) {
						fprintf(stderr, "client_handling(%d) error: %d \n",
								epev->data.fd, ret);
						delete_epoll_fd(epfd, epev->data.fd);
					}
 				} else if (epev->events & (EPOLLHUP | EPOLLRDHUP)) {
					printf("shutdown client: %d \n", epev->data.fd);
					delete_epoll_fd(epfd, epev->data.fd);
				}
			}
		} while ((*++epev).data.ptr);
	} // while (true)

	return 0;
}

#define EXTRACT(ptr, value) memcpy(&value, ptr, sizeof(value)), ptr += sizeof(value);
#define LIMITS(value, max) ((value) < (max) ? (value) : (max))

int client_handling(int sock)
{
	uint32_t command;
	char data[HEADER_SIZE];
	size_t dsize;

	if ((dsize = recv(sock, (char *)data, HEADER_SIZE, MSG_PEEK)) == -1)
		return -1;

	if (is_http_header((const char *)data)) {
		struct http_header http;
		int device_id, device_sock;
		uint32_t length;
		int clnt_sock = sock;
		char buffer[BUFSIZ];

		printf("HTTP data \n");

		do {
			int ret;
			if ((ret = recv_until(sock, data, HEADER_SIZE, "\r\n\r\n")) < 0)
				return -2;
			dsize = ret;
		} while (false);

		if (parse_http_header(data, dsize, &http) < 0)
			return -3;

		show_http_header(&http);

		if (sscanf(http.url, "/%d", &device_id) != 1)
			return -4;

		device_sock = find_device(device_id);
		if (device_sock < 0)
			return -5;

		if (sscanf(http.content.length, "%d", &length) != 1)
			return -6;

		command = IPC_RECEIVED_CLIENT;

		if (send(device_sock, &command,
				 sizeof(command), MSG_DONTWAIT) != sizeof(command))
			return -7;

		if (send(device_sock, &length,
				 sizeof(length), MSG_DONTWAIT) != sizeof(length))
			return -8;

		for (int received = 0, to_read; received < length; received += to_read)
		{
			if ((to_read = recvt(clnt_sock, buffer,
						   LIMITS(length - received, sizeof(buffer)), CLOCKS_PER_SEC)) < 0)
				return -9;

			if (to_read == 0) break;

			if (sendt(device_sock, buffer, to_read, CLOCKS_PER_SEC) < 0)
				return -10;
		}

		send_response(clnt_sock, 200);
		close(clnt_sock);

		printf("send successfully \n");
	} else {
		size_t hsize;
		uint32_t command;
		char *dptr = data;

		if ((hsize = recvt(sock, (char *)data, HEADER_SIZE, MSG_DONTWAIT)) == -1)
			return -1;

		printf("binary data \n");
		EXTRACT(dptr, command);

		switch (command) {
		case IPC_REGISTER_DEVICE: {
			uint32_t device_id;

			EXTRACT(dptr, device_id);

			printf("Register device: %u <=> %u \n", sock, device_id);

			register_device(sock, device_id);
			break;
		}
		default: return -1;
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
