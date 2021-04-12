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
#include <pthread.h>

#define HEADER_SIZE 1024
#define WORKER_THREADS_SIZE 1
#define HTTP_URL_LENGTH 128

#include "device_manager.h"

#define stringify(x) #x
#define UNION_LIBRARY(NAME) stringify(u_ ## NAME)

#include UNION_LIBRARY(utils.h)
#include UNION_LIBRARY(ipc_manager.h)
#include UNION_LIBRARY(logger.h)
#include UNION_LIBRARY(json.h)

struct thread_arguments {
	int epoll_fd;
	int id;
};

enum HTTP_METHOD {
	HTTP_METHOD_POST,
	HTTP_METHOD_GET,
	HTTP_METHOD_DELETE,
	HTTP_METHOD_PETCH
};

struct http_header {
	int method;
	char url[HTTP_URL_LENGTH + 1];
	int length;
};

struct epoll_structure {
	int fd;
	int type;

	struct http_header http;

	uint8_t *body;
	size_t received;
};

void *worker_thread(void *args);
int client_handling(int epfd, struct epoll_structure *clnt_strt);

int main(int argc, char *argv[])
{
	struct thread_arguments thd_arg;
	pthread_t worker[WORKER_THREADS_SIZE];
	int serv_sock, epfd;
	int ret;
	struct device *device;

	if (argc != 3)
		logg(LOG_CRI, "usage: <%s> <port> <backlog>", argv[0]);

	if ((serv_sock = make_listener(strtol(argv[1], NULL, 10),
								   strtol(argv[2], NULL, 10), true)) < 0)
		logg(LOG_CRI, "failed to make listener %d", serv_sock);

	logg(LOG_INF, "create server %d", serv_sock);

	if ((epfd = epoll_create1(0)) == -1)
		logg(LOG_CRI, "epoll_create() error");

	logg(LOG_INF, "create epoll %d", epfd);

	if ((device = init_device()) == NULL)
		logg(LOG_CRI, "init_device() error");

	logg(LOG_INF, "initialize device data structure");

	for (int i = 0; i < WORKER_THREADS_SIZE; i++) {
		struct thread_arguments *thd_arg;
		thd_arg = (struct thread_arguments *) malloc(sizeof(struct thread_arguments));
		if (thd_arg == NULL) logg(LOG_CRI, "failed to allocate thread_arguments");
		else *thd_arg = (struct thread_arguments) {
			.epoll_fd = epfd,
			.id = (i + 1)
		};

		if (pthread_create(&worker[i], NULL, worker_thread, thd_arg) != 0)
			logg(LOG_CRI, "failed to create worker threads");
	}

	logg(LOG_INF, "create worker threads %d", WORKER_THREADS_SIZE);

	while (true) {
		struct sockaddr_in clnt_adr;
		struct epoll_structure *epst;
		socklen_t clnt_addr_size;

		int clnt_sock = accept(serv_sock, (struct sockaddr *) &clnt_adr, &clnt_addr_size);
		if (clnt_sock < 0) {
			logg(LOG_ERR, "failed to accept client: %s", strerror(errno));
			continue;
		} else logg(LOG_INF, "accept client %d", clnt_sock);

		if (change_flag(clnt_sock, O_NONBLOCK) < 0) {
			logg(LOG_ERR, "failed to set clnt_sock flag");
			close(clnt_sock);
			continue;
		}

		epst = malloc(sizeof(struct epoll_structure));
		if (epst == NULL) {
			logg(LOG_ERR, "failed to create epoll_structure");
			close(clnt_sock);
			continue;
		} else *epst = (struct epoll_structure) {
			.fd = clnt_sock,
			.type = -1,
			.body = NULL
		};

		if (register_epoll_fd(epfd, clnt_sock, epst, EPOLLIN | EPOLLONESHOT) == -1) {
			logg(LOG_ERR, "failed to register client to epoll");
			continue;
		} else logg(LOG_ERR, "register client to epoll");
	}

	close(epfd);
	close(serv_sock);

	return 0;
}

void *worker_thread(void *args)
{
	struct thread_arguments thd_arg = *(struct thread_arguments *) args;
	struct epoll_structure *epst;
	int ret;

	free(args);

	for (struct epoll_event events[MAX_EVENT], *epev;
		 (epev = wait_epoll_event(thd_arg.epoll_fd, events, MAX_EVENT, -1)) != NULL;)
	{
		logg(LOG_INF, "event occured in thread %zu", thd_arg.id);
		do {
			if (epev->data.ptr == NULL) break;
			else epst = epev->data.ptr;

			if (epev->events & EPOLLIN) {
				if (recv(epst->fd, (char []) { 0x00 }, 1, MSG_PEEK) <= 0)
					goto DELETE;

				if ((ret = client_handling(thd_arg.epoll_fd, epst)) < 0) {
					logg(LOG_ERR, "failed to handling client: %d %s", ret, strerror(errno));
					goto DELETE;
				}

			} else if (epev->events & (EPOLLHUP | EPOLLRDHUP)) {
				goto DELETE;
			} else goto DELETE;

			continue; DELETE:
			logg(LOG_INF, "disconnect client: %d", epst->fd);
			delete_epoll_fd(thd_arg.epoll_fd, epst->fd);
			if (epst->body != NULL)
				free(epst->body);
			free(epst);
		} while ((*++epev).data.ptr);
	}

	return 0;
}

bool is_http_header1(const char *header)
{
	if (strstr(header, "HTTP") && strstr(header, "\r\n\r\n"))
		return true;

	return false;
}

int parse_http_header1(char *raw, struct http_header *http)
{
	char method[10];
	char *ptr;

	if (sscanf(raw, "%10s %128s", method, http->url) != 2)
		return -1;

	ptr = strstr(raw, "Content-Length:");
	if (ptr == NULL) {
		return -2;
	} else if (sscanf(ptr, "Content-Length: %d", &http->length) != 1) {
		return -3;
	}

	if (!strncmp(method, "POST", 4)) {
		http->method = 1;
	} else if (!strncmp(method, "GET", 3)) {
		http->method = 2;
	} else if (!strncmp(method, "DELETE", 6)) {
		http->method = 3;
	} else if (!strncmp(method, "PETCH", 5)) {
		http->method = 4;
	} else return -4;

	return 0;
}

int client_handling(int epfd, struct epoll_structure *clnt_strt)
{
	char header[HEADER_SIZE];

	if (clnt_strt->type == -1) {
		if (recv(clnt_strt->fd, header, HEADER_SIZE, MSG_PEEK) < 0)
			return -1;

		if (is_http_header1(header)) {
			logg(LOG_INF, "HTTP Request");
			int received;
			if ((received = recv_until(clnt_strt->fd, header, HEADER_SIZE, "\r\n\r\n")) < 0)
				return -2;

			if (parse_http_header1(header, &clnt_strt->http) < 0)
				return -3;

			clnt_strt->body = malloc(sizeof(clnt_strt->http.length));
			if (clnt_strt->body == NULL)
				return -4;
			printf("clnt_strt->body: %p\n", clnt_strt->body);

			clnt_strt->received = 0;
			clnt_strt->type = 1;
		}
	} else if (clnt_strt->type == 1) {
		int rcv = 0;
		int cur = clnt_strt->received;
		int len = clnt_strt->http.length;
		int to_recv;
		char buffer[1024];

		if (cur == len) {
			logg(LOG_INF, "read all HTTP body");
		} else to_recv = (len - cur) > 1024 ? 1024 : len - cur;

		printf("clnt_strt->body + cur: %p\n", clnt_strt->body + cur);
		if (recv(clnt_strt->fd, clnt_strt->body + cur, to_recv, 0) != to_recv)
		{
			return -5;
		} else logg(LOG_INF, "Progress: (%d + %d) / %d ", cur, to_recv, len);

		clnt_strt->received += to_recv;
	}

	if (change_epoll_event(epfd, clnt_strt->fd, clnt_strt, EPOLLIN | EPOLLONESHOT) < 0)
		return -6;

	return 0;
}

/*
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
			} else {
				if (epev->events & EPOLLIN) {
					int ret;

					if (recv(epev->data.fd, (char []) { 0x00 }, 1, MSG_PEEK) <= 0)
						goto DELETE;

					if ((ret = client_handling(epev->data.fd, device)) < 0)
						goto DELETE;

					logg(LOG_INF, "client_handling(%d) %d", epev->data.fd, ret);
 				} else if (epev->events & (EPOLLHUP | EPOLLRDHUP)) {
					logg(LOG_INF, "hang-up client event occured");
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
		struct packet_header packet = { .bsize = -1 };
		char json_str[BUFSIZ], number[10];
		struct json *json;
		int32_t len;

		logg(LOG_INF, "HTTP request from %d", sock);
		ret = http_client(sock, device, &packet);

		logg(((ret < 0) ? LOG_ERR : LOG_INF), "http_client() %d", ret);

		if (packet.bsize > 0) {
			char *body = malloc(sizeof(packet.bsize));
			if (body == NULL)
				return -3;
		}

		json = MAKE_JSON("result", ret, NULL);
		stringify_json(json, json_str);
		if (delete_json(json) != 1)
			return -2;

		len = make_http_header_s(header, HEADER_SIZE, 200, "application/json", strlen(json_str));
		if (sendt(sock, header, len, CPS) <= 0)
			return -3;

		if (sendt(sock, json_str, strlen(json_str), CPS) <= 0)
			return -4;
	} else {
		logg(LOG_INF, "binary request from %d", sock);
		if ((ret = device_client(sock, header, device)) < 0)
			logg(LOG_ERR, "failed to device_client() %d", ret);

		if (sendt(sock, &ret, sizeof(int32_t), CPS) <= 0)
			return -5;
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

		if (recvt(sock, *body, packet->bsize, CPS * 10) <= 0)
			return -5;
	} else *body = NULL;

	if (http.url == NULL)
		return -6;

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

	case GET: ;
		int type;
		if (sscanf(http.url, "/%d", &type) != 1)
			return -10;

		switch (type) {
		case 0: packet->method = IPC_GET_PRINTER_STATUS;
				break;
		case 1: packet->method = IPC_GET_PRINTER_BEHAVIOR;
				break;
		case 2: packet->method = IPC_GET_SCHEDULER;
				break;
		case 3: packet->method = IPC_GET_SCREENSHOT;
				break;
		}

		break;

	default:
		return -11;
	}

	return 0;
}

int http_client(int clnt_sock, struct device *device, struct packet_header *recv_packet)
{
	struct packet_header send_packet;

	int dev_sock;
	char *body;

	int32_t ret;

	if ((ret = http_to_packet(clnt_sock, &send_packet, &body)) < 0)
		return ret;

	if (!check_device_key(device, send_packet.device_id, send_packet.device_key))
		return -51;

	dev_sock = find_device_sock(device, send_packet.device_id);
	if (dev_sock < 0)
	{	free(body); return -52;	}

	if (sendt(dev_sock, &send_packet, sizeof(send_packet), CPS) <= 0)
	{	free(body); return -53;	}

	if (send_packet.bsize > 0)
		if (sendt(dev_sock, body, send_packet.bsize, CPS * 10) <= 0)
		{	free(body); return -54;	}

	free(body);

	if (recvt(dev_sock, recv_packet, sizeof(struct packet_header), CPS * 3) <= 0)
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
*/
