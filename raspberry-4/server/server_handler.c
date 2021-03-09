#include "server_handler.h"

int register_epoll_client(int epfd, int serv_sock, int flags)
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

		printf("connect client: %d \n", clnt_sock);

		count++;
	}

	return count;
}

int make_server(short port, int backlog)
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

struct epoll_event *wait_epoll_event(int epfd, int maxevent, int timeout)
{
	static struct epoll_event events[MAX_EVENT];
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
	char buffer[BUFSIZ];

	while (recv(sock, buffer, sizeof(buffer), 0) != -1)
		/* empty loop body */ ;

	return -(errno == EAGAIN);
}
