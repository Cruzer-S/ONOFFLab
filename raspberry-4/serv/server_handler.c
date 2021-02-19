#include "server_handler.h"

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
	int ret;

	switch (flag) {
	case SO_RCVTIMEO: ;
		struct timeval tv = {
			.tv_sec = 5,
			.tv_usec = 5
		};

		ret = setsockopt(fd, level, flag, &tv, sizeof(tv)); 
		break;

	default:
		ret = setsockopt(fd, level, flag, &value, sizeof(value));
		break;
	}


	if (ret == -1)
		return -1;

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
