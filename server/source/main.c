#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <inttypes.h>
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

int http_client(int clnt_sock, struct device *device);
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

					logg(LOG_INF, "client_handling(%d) %d", epev->data.fd, ret);
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
	int32_t ret;
	char header[HEADER_SIZE];

	if (recv(sock, (char *)header, HEADER_SIZE, MSG_PEEK) <= 0)
		return -1;

	if (is_http_header((const char *)header)) {
		char json[BUFSIZ], number[10];
		int32_t len;

		logg(LOG_INF, "HTTP request from %d", sock);
		ret = http_client(sock, device);

		logg(((ret < 0) ? LOG_ERR : LOG_INF), "http_client() error %d", ret);

		ITOS(ret, number); make_json(1, json, "result", number);

		len = make_http_header_s(header, HEADER_SIZE, 200, "application/json", strlen(json));
		if (sendt(sock, header, len, CPS / 2) <= 0)
			return -2;

		if (sendt(sock, json, strlen(json), CPS / 2) <= 0)
			return -3;
	} else {
		logg(LOG_INF, "binary request from %d", sock);
		if ((ret = device_client(sock, header, device)) < 0)
			logg(LOG_ERR, "failed to device_client() %d", ret);

		if (sendt(sock, &ret, sizeof(int32_t), CPS) <= 0)
			return -4;
	}

	return ret;
}

int http_to_packet(int sock, struct packet_header *packet, char **body)
{
	struct http_header http;
	char header[HEADER_SIZE];
	int32_t hsize;

	*packet = (struct packet_header) {
		.method = -1, .bsize = -1,
		.fname = { 0, }, .rname = { 0, },
		.quantity = -1, .order = -1,
		.device_id = -1, .device_key = { 0, }
	};

	if ((hsize = recv_until(sock, header, HEADER_SIZE, "\r\n\r\n")) < 0)
		return -1;

	if (parse_http_header(header, hsize, &http) < 0)
		return -2;

	if (http.content.length) {
		if (sscanf(http.content.length, "%d", &packet->bsize) != 1)
			return -3;

		if ((*body = (char *)malloc(sizeof(char) * packet->bsize)) == NULL)
			return -4;

		if (recvt(sock, *body, packet->bsize, CPS) <= 0)
			return -5;
	} else *body = NULL;

	if (http.url == NULL)
		return -6;

	/********************************************************************************
	 * Warning: sscanf format string uses a constant value, not a variable.         *
	 * YOU MUST CHECK member variables OUT how long they are.                       *
	 ********************************************************************************/
	if (sscanf(http.url, "/%8" PRId32 "/%32[^/]", &packet->device_id, packet->device_key) != 2)
		return -7;

	switch (parse_string_method(http.method))
	{
	case POST: packet->method = IPC_REGISTER_GCODE;
		if (sscanf(http.url, "/%*8" PRId32 "/%*32[^/]/%128[^/]/%d", packet->fname, &packet->quantity) != 2)
			return -8;

		break;

	case DELETE:
		packet->method = IPC_CHANGE_QUANTITY_AND_ORDER;
		if (sscanf(http.url, "/%*8" PRId32 "/%*32[^/]/%128[^/]/%d/%d",
				   packet->fname, &packet->quantity, &packet->order) != 3) {

			packet->method = IPC_RENAME_GCODE;
			if (sscanf(http.url, "/%*8" PRId32 "/%*32[^/]/%128[^/]/%d/%d",
					   packet->fname, &packet->quantity, &packet->order) != 3) {
				packet->method = IPC_DELETE_GCODE;

				if (sscanf(http.url, "/%*8" PRId32 "/%*32[^/]/%128[^/]", packet->fname) != 1)
					return -9;
			}
		}
		break;

	default:
		return -10;
	}

	return 0;
}

int http_client(int clnt_sock, struct device *device)
{
	struct packet_header packet;

	int dev_sock;
	char *body;

	int32_t ret;

	if ((ret = http_to_packet(clnt_sock, &packet, &body)) < 0)
		return ret;

	if (!check_device_key(device, packet.device_id, packet.device_key))
		return -51;

	dev_sock = find_device_sock(device, packet.device_id);
	if (dev_sock < 0)
	{	free(body); return -52;	}

	if (sendt(dev_sock, &packet, sizeof(packet), CPS) <= 0)
	{	free(body); return -53;	}

	if (packet.bsize > 0)
		if (sendt(dev_sock, body, packet.bsize, CPS) <= 0)
		{	free(body); return -54;	}

	free(body);

	if (recvt(dev_sock, &ret, sizeof(int32_t), CPS) <= 0)
		return -55;

	return (ret * 100);
}

int device_client(int device_sock, char *data, struct device *device)
{
	struct packet_header packet;
	int hsize;

	if (recvt(device_sock, &packet, PACKET_SIZE, MSG_DONTWAIT) != PACKET_SIZE)
		return -1;

	logg(LOG_INF, "request: %d", packet.method);

	switch (packet.method) {
	case IPC_REGISTER_DEVICE:
		logg(LOG_INF, "bind socket to device %d (%d:%s)",
			 device_sock, packet.device_id, packet.device_key);

		if (!insert_device(device, device_sock, packet.device_id, packet.device_key))
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

