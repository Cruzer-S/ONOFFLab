#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>

#include "server_handler.h"
#include "device_manager.h"
#include "http_handler.h"
#include "json_parser.h"
#include "utils.h"

_Noreturn void error_handling(const char *format, ...);
int client_handling(int sock, struct device *device);
int start_worker_thread(int epfd, int serv_sock, struct device *device);

int main(int argc, char *argv[])
{
	int serv_sock, epfd;
	int ret;
	struct device *device;

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

	if ((device = init_device()) == NULL)
		error_handling("init_device() error \n");

	printf("start server \n");

	while ((ret = start_worker_thread(epfd, serv_sock, device)) < 0)
		fprintf(stderr , "start_epoll_thread() error: %d", ret);

	close(epfd);
	close(serv_sock);

	return 0;
}

int start_worker_thread(int epfd, int serv_sock, struct device *device)
{
	while (true)
	{
		struct epoll_event *epev = wait_epoll_event(epfd, MAX_EVENT, -1);
		if (epev == NULL) return -1;

		if (epev->data.ptr == NULL) break;

		do {
			if (epev->data.fd == serv_sock) {
				int cnt = 0;
				if ((cnt = accept_epoll_client(epfd, serv_sock, EPOLLIN | EPOLLET)) < 1) {
					fprintf(stderr, "accept_epoll_client() error \n");
					continue;
				}

				printf("accepting %d client(s) \n", cnt);
			} else { // client
				if (epev->events & EPOLLIN) {
					int ret;

					if (recv(epev->data.fd, (char []) { 0x00 }, 1, MSG_PEEK) <= 0) {
						printf("disconnect client: %d \n", epev->data.fd);
						goto DELETE;
					}

					printf("receive data from client: %d \n", epev->data.fd);
					if ((ret = client_handling(epev->data.fd, device)) < 0) {
						fprintf(stderr, "client_handling(%d) error: %d \n",
								epev->data.fd, ret);
						goto DELETE;
					}
 				} else if (epev->events & (EPOLLHUP | EPOLLRDHUP)) {
					printf("shutdown client: %d \n", epev->data.fd);
					goto DELETE;
				}

				continue; DELETE:
				delete_epoll_fd(epfd, epev->data.fd);
				delete_device_sock(device, epev->data.fd);
			}
		} while ((*++epev).data.ptr);
	}

	return 0;
}

int http_client(int clnt_sock, char *header, struct device *device)
{
	struct http_header http;
	char buffer[BUFSIZ];
	int hsize, bsize;
	int32_t device_id, device_sock;

	do {
		int ret;
		if ((ret = recv_until(clnt_sock, header, HEADER_SIZE, "\r\n\r\n")) < 0)
			return -1;
		hsize = ret;
	} while (false);

	if (parse_http_header(header, hsize, &http) < 0)
		return -2;

	printf("http method: %s\n", http.method);

	if (http.url == NULL)
		return -3;

	if (sscanf(http.url, "/%d", &device_id) != 1)
		return -4;

	device_sock = find_device_sock(device, device_id);
	if (device_sock < 0)
		return -5;

	if (http.content.length)
		if (sscanf(http.content.length, "%d", &bsize) != 1)
			return -6;

	switch (parse_string_method(http.method)) {
	case POST: // IPC_REGISTER_GCODE
		if (sendt(device_sock, (int32_t []) { IPC_REGISTER_GCODE },
				  sizeof(int32_t), CLOCKS_PER_SEC) < 0)
			return -7;

		if (sendt(device_sock, &bsize, sizeof(bsize), CLOCKS_PER_SEC) < 0)
			return -8;

		for (int received = 0, to_read; received < bsize; received += to_read)
		{
			if ((to_read = recvt(clnt_sock, buffer,
						   LIMITS(bsize - received, sizeof(buffer)), CLOCKS_PER_SEC)) < 0)
				return -9;

			if (to_read == 0) break;

			if (sendt(device_sock, buffer, to_read, CLOCKS_PER_SEC) < 0)
				return -10;
		}

		break;
	default: return -11;
	}

	int32_t ret;
	if (recvt(device_sock, &ret, sizeof(int32_t), CLOCKS_PER_SEC) < 0)
		return -12;

	printf("ret: %d \n", ret);

	return ret;
}

int device_client(int device_sock, char *data, struct device *device)
{
	int hsize;
	int32_t command;
	int32_t device_id;

	if ((hsize = recvt(device_sock, (char *)data, HEADER_SIZE, MSG_DONTWAIT)) < 0)
		return -1;

	EXTRACT(data, command);

	switch (command) {
	case IPC_REGISTER_DEVICE:

		EXTRACT(data, device_id);

		if (!insert_device(device, device_id, device_sock))
			return -2;

		if (change_sockopt(device_sock, SOL_SOCKET, SO_KEEPALIVE, 1) < 0)
			return -3;

		if (change_sockopt(device_sock, IPPROTO_TCP, TCP_KEEPIDLE, 60) < 0)
			return -4;

		if (change_sockopt(device_sock, IPPROTO_TCP, TCP_KEEPCNT, 1) < 0)
			return -5;

		if (change_sockopt(device_sock, IPPROTO_TCP, TCP_KEEPINTVL, 60) < 0)
			return -6;

		printf("Register device: %u <=> %u \n", device_sock, device_id);
		break;

	default: return -6;
	}

	return 0;
}

int client_handling(int sock, struct device *device)
{
	int32_t command;
	char data[HEADER_SIZE];
	size_t dsize;
	int ret;

	if ((dsize = recv(sock, (char *)data, HEADER_SIZE, MSG_PEEK)) <= 0)
		return -1;

	if (is_http_header((const char *)data)) {
		printf("HTTP data \n");

		if ((ret = http_client(sock, data, device)) < 0) {
			printf("failed to http_client(): %d \n", ret);
			send_response(sock, 404);
			return -2;
		}

		char *json = make_json(1, "result", itos(ret));
		if (json == NULL)
			return -3;

		int len = make_http_header_s(data, HEADER_SIZE, 200, "application/json", strlen(json));

		if (sendt(sock, data, len, CLOCKS_PER_SEC) < 0)
			return -4;

		if (sendt(sock, json, strlen(json), CLOCKS_PER_SEC) < 0)
			return -5;

		free(json);

		shutdown(sock, SHUT_RD);
	} else {
		printf("binary data \n");

		if ((ret = device_client(sock, data, device)) < 0) {
			printf("failed to device_client(): %d \n", ret);
			return -2;
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
