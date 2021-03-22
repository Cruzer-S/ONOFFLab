#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>

#define stringify(x) #x
#define UNION_LIBRARY(NAME) stringify(u_ ## NAME)

#include UNION_LIBRARY(utils.h)
#include UNION_LIBRARY(ipc_manager.h)
#include UNION_LIBRARY(logger.h)

#include "device_manager.h"
#include "http_handler.h"
#include "json_parser.h"

int client_handling(int sock, struct device *device);
int worker_thread(int epfd, int serv_sock, struct device *device);

int http_client(int clnt_sock, char *header, struct device *device);
int device_client(int device_sock, char *data, struct device *device);

int main(int argc, char *argv[])
{
	int serv_sock, epfd;
	int ret;
	struct device *device;

	if (argc != 3)
		logg(LOG_CRI, "usage: <%s> <port> <backlog>", argv[0]);

	if ((serv_sock = make_listener(strtol(argv[1], NULL, 10),
								 strtol(argv[2], NULL, 10))) < 0)
		logg(LOG_CRI, "failed to make listener %d", serv_sock);

	logg(LOG_INF, "create server %d", serv_sock);

	if ((ret = change_flag(serv_sock, O_NONBLOCK)) < 0)
		logg(LOG_CRI, "chage_flag() %d", ret);

	if (change_sockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, true) == -1)
		logg(LOG_CRI, "change_sockopt() error");

	logg(LOG_INF, "change server socket option");

	if ((epfd = epoll_create1(0)) == -1)
		logg(LOG_CRI, "epoll_create() error");

	if (register_epoll_fd(epfd, serv_sock, EPOLLIN | EPOLLET) == -1)
		logg(LOG_CRI, "register_epoll_fd() error");

	logg(LOG_INF, "create epoll");

	if ((device = init_device()) == NULL)
		logg(LOG_CRI, "init_device() error");

	logg(LOG_INF, "initialize device data structure");

	while ((ret = worker_thread(epfd, serv_sock, device)) < 0)
		logg(LOG_ERR, "worker_thread() error %d", ret);

	close(epfd);
	close(serv_sock);

	return 0;
}

int worker_thread(int epfd, int serv_sock, struct device *device)
{
	for (struct epoll_event events[MAX_EVENT], *epev;
		 (epev = wait_epoll_event(epfd, events, MAX_EVENT, -1)) != NULL;)
	{
		do {
			if (epev->data.ptr == NULL) break;

			if (epev->data.fd == serv_sock) {
				int cnt = 0;
				if ((cnt = accept_epoll_client(epfd, serv_sock, EPOLLIN | EPOLLET)) < 1) {
					logg(LOG_ERR, "accept_epoll_client() error");
					continue;
				}

				logg(LOG_INF, "accept %d client(s) ", cnt);
			} else { // client
				if (epev->events & EPOLLIN) {
					int ret;

					if (recv(epev->data.fd, (char []) { 0x00 }, 1, MSG_PEEK) <= 0)
						goto DELETE;

					if ((ret = client_handling(epev->data.fd, device)) < 0)
						goto DELETE;

					logg(LOG_INF, "client_handling(%d): %d", epev->data.fd, ret);
 				} else if (epev->events & (EPOLLHUP | EPOLLRDHUP)) {
					goto DELETE;
				}

				continue; DELETE:
				logg(LOG_INF, "disconnect client %d", epev->data.fd);

				delete_epoll_fd(epfd, epev->data.fd);
				delete_device_sock(device, epev->data.fd);
			}
		} while ((*++epev).data.ptr);
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
		logg(LOG_INF, "HTTP request from %d", sock);
		if ((ret = http_client(sock, data, device)) < 0)
			logg(LOG_ERR, "http_client() error %d", ret);

		char json[BUFSIZ], number[10];
		ITOS(ret, number);
		make_json(1, json, "result", number);

		flush_socket(sock);

		int len = make_http_header_s(data, HEADER_SIZE, 200, "application/json", strlen(json));
		if (sendt(sock, data, len, CLOCKS_PER_SEC) < 0)
			return -2;

		if (sendt(sock, json, strlen(json), (CLOCKS_PER_SEC * 2)) < 0)
			return -3;
	} else {
		logg(LOG_INF, "binary request from %d", sock);
		if ((ret = device_client(sock, data, device)) < 0)
			logg(LOG_ERR, "failed to device_client() %d", ret);

		if (sendt(sock, &ret, sizeof(int32_t), CPS) < 0)
			return -4;
	}

	return ret;
}

int http_client(int clnt_sock, char *header, struct device *device)
{
	struct http_header http;
	char buffer[BUFFER_SIZE];
	int hsize, bsize;
	int32_t device_id, device_sock, ret;

	if ((ret = recv_until(clnt_sock, header, HEADER_SIZE, "\r\n\r\n")) < 0)
		return -1;
	else hsize = ret;

	if (parse_http_header(header, hsize, &http) < 0)
		return -2;

	if (http.url == NULL) return -3;

	if (sscanf(http.url, "/%d", &device_id) != 1)
		return -4;

	device_sock = find_device_sock(device, device_id);
	if (device_sock < 0) return -5;

	if (http.content.length)
		if (sscanf(http.content.length, "%d", &bsize) != 1)
			return -6;

	switch (parse_string_method(http.method)) {
	case POST: // IPC_REGISTER_GCODE
		logg(LOG_INF, "receive gcode from client %d", clnt_sock);
		logg(LOG_INF, "send gcode to device id %d (%d)", device_id, device_sock);

		if (sendt(device_sock, (int32_t []) { IPC_REGISTER_GCODE },
				  sizeof(int32_t), CLOCKS_PER_SEC) < 0)
			return -7;

		int32_t sign;
		if (recvt(device_sock, &sign, sizeof(sign), CPS) < 0)
			return -8;

		if (sign < 0) return (sign * 100);

		if (sendt(device_sock, &bsize, sizeof(bsize), CLOCKS_PER_SEC) < 0)
			return -9;

		for (int received = 0, to_read; received < bsize; received += to_read)
		{
			if ((to_read = recvt(clnt_sock, buffer,
						   LIMITS(bsize - received, sizeof(buffer)), CLOCKS_PER_SEC)) < 0)
				return -10;

			if (to_read == 0) break;

			if (sendt(device_sock, buffer, to_read, CLOCKS_PER_SEC) < 0)
				return -11;
		}

		break;
	default: return -12;
	}

	if (recvt(device_sock, &ret, sizeof(int32_t), CLOCKS_PER_SEC) < 0)
		return -12;

	return (ret * 100);
}

int device_client(int device_sock, char *data, struct device *device)
{
	int hsize;
	int32_t command;
	int32_t device_id;

	if ((hsize = recvt(device_sock, (char *)data, HEADER_SIZE, MSG_DONTWAIT)) < 0)
		return -1;

	data = EXTRACT(data, command);
	logg(LOG_INF, "request: %d", command);

	switch (command) {
	case IPC_REGISTER_DEVICE:
		data = EXTRACT(data, device_id);
		logg(LOG_INF, "bind socket to device (%d to %d)", device_sock, device_id);

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
		break;

	default: return -7;
	}

	return 0;
}

