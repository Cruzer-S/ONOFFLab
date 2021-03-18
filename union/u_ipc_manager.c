#include "u_ipc_manager.h"
#include <asm-generic/socket.h>

int accept_epoll_client(int epfd, int serv_sock, int flags)
{
	int count = 0;
	while (true) {
		struct sockaddr_in clnt_adr;
		int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, (socklen_t []) { sizeof clnt_adr });

		if (clnt_sock == -1) {
			if (errno == EAGAIN) break;

			fprintf(stderr, "failed to accept client \n");
			continue;
		}

		change_flag(clnt_sock, O_NONBLOCK);

		if (register_epoll_fd(epfd, clnt_sock, EPOLLIN | EPOLLET) == -1) {
			fprintf(stderr, "register_epoll_fd(clnt_sock) error \n");
			continue;
		}

		count++;
	}

	return count;
}

int connect_to_target(const char *host, uint16_t port)
{
	static struct sockaddr_in sock_adr;
	struct hostent *entry;

	int sock;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
		return -1;

	if (host) {
		entry = gethostbyname(host);

		memset(&sock_adr, 0x00, sizeof(sock_adr));

		sock_adr.sin_family = AF_INET;
		sock_adr.sin_port = htons(port);

		if (entry) {
			memcpy(&sock_adr.sin_addr, entry->h_addr_list[0], entry->h_length);
		} else { // Failed to convert hostname,
				 // it may means that host will be ip address
			sock_adr.sin_addr.s_addr = inet_addr(host);
		}
	}

	if (connect(sock, (struct sockaddr *)&sock_adr, sizeof(sock_adr)) < 0) {
		close(sock);
		return -2;
	}

	return sock;
}

int make_listener(short port, int backlog)
{
	struct sockaddr_in sock_adr;
	int sock;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
		return -1;

	memset(&sock_adr, 0, sizeof(sock_adr));
	sock_adr.sin_family = AF_INET;
	sock_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_adr.sin_port = htons(port);

	if (bind(sock, (struct sockaddr *)&sock_adr, sizeof(sock_adr)) == -1)
		return -2;

	if (listen(sock, backlog) == -1)
		return -3;

	return sock;
}

int change_flag(int fd, int flag)
{
	int flags = fcntl(fd, F_GETFL);

	if (flags == -1)
		return -1;

	flags |= flag;

	if (fcntl(fd, F_SETFL, flags) == -1)
		return -2;

	return 0;
}

int change_sockopt(int fd, int level, int flag, int value)
{
	switch (flag) {
	case SO_RCVTIMEO: ;
		struct timeval tv = {
			.tv_sec = value / 1000,
			.tv_usec = value % 1000
		};

		return setsockopt(fd, level, flag, &tv, sizeof(tv));

	default:
		return setsockopt(fd, level, flag, &value, sizeof(value));
	}

	return 0;
}

int register_epoll_fd(int epfd, int tgfd, int flag)
{
	struct epoll_event event;

	event.events = flag;
	event.data.fd = tgfd;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, tgfd, &event) == -1)
		return -1;

	return 0;
}

struct epoll_event *wait_epoll_event(int epfd, struct epoll_event *events, int maxevent, int timeout)
{
	int count;

	count = epoll_wait(epfd, events, maxevent, timeout);
	if (count == -1)
		return NULL;

	events[count].data.ptr = NULL;

	return events;
}

int delete_epoll_fd(int epfd, int tgfd)
{
	int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, tgfd, NULL);

	close(tgfd);

	return ret;
}

int flush_socket(int sock)
{
	char buffer[BUFFER_SIZE];

	while (recv(sock, buffer, sizeof(buffer), MSG_DONTWAIT) != -1)
		/* empty loop body */ ;

	return -(errno == EAGAIN);
}

int recv_until(int sock, char *buffer, int bsize, char *end)
{
	char *next;
	int received;

	for (received = 0, next = end;
		 recv(sock, buffer + received, 1, MSG_DONTWAIT) == 1
	  && received < bsize;
	     received++)
	{
		if (*next == *(buffer + received))
			next++;
		else next = end;

		if (*next == '\0') return received;
	}

	return -1;
}

int recvt(int sock, void *buffer, int size, clock_t timeout)
{
	int received = 0, ret;
	clock_t start = clock(), end = start;

	for (end = start; end - start < timeout && received < size; end = clock())
	{
		ret = recv(sock, buffer + received, size - received, MSG_DONTWAIT);
		if (ret == 0) return -3;

		if (ret == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
			else return -1;
		} else received += ret;
	}

	return received;
}

int sendt(int sock, void *buffer, int size, clock_t timeout)
{
	int to_send = 0, ret;
	clock_t start = clock(), end = start;

	for (end = start; end - start < timeout && to_send < size; end = clock())
	{
		ret = send(sock, buffer + to_send, size - to_send, MSG_DONTWAIT);
		if (ret == 0) return -3;

		if (ret == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
			else return -1;
		} else to_send += ret;
	}

	return to_send;
}

int readall(int sock, char *buffer, int length)
{
	int ret;
	int received;

	for (received = ret = 0;
		 ret != -1 && received < length;
		 received += (ret = recv(sock, buffer + received, length - received, MSG_DONTWAIT)));

	if (ret == -1 && errno != EAGAIN)
		return -1;

	return received;
}
