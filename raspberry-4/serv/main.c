#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "server_handler.h"

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

	epfd = epoll_create(1024);
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
				while (true) {
					struct sockaddr_in clnt_adr;
					int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, (socklen_t []) { sizeof clnt_adr });

					if (clnt_sock == -1) {
						if (errno == EAGAIN) break;

						fprintf(stderr, "failed to accept client \n");
						continue;
					}

					if (register_epoll_fd(epfd, clnt_sock, EPOLLIN | EPOLLET | EPOLLONESHOT) == -1) {
						fprintf(stderr, "register_epoll_fd(clnt_sock) error \n");
						continue;
					}

					printf("connect client: %d \n", clnt_sock);
				}
			} else { // client
				if (epev->events & EPOLLIN) {
					if (recv(epev->data.fd, (char []) { 0x00 }, 1, MSG_PEEK) == 0) {
						printf("disconnect client: %d \n", epev->data.fd);
						delete_epoll_fd(epfd, epev->data.fd);
						continue;
					} else printf("receive from client: %d \n", epev->data.fd);

					client_handling(epev->data.fd);
 				} else if (epev->events & (EPOLLHUP | EPOLLRDHUP)) {
					printf("shutdown client: %d \n", epev->data.fd);
					delete_epoll_fd(epfd, epev->data.fd);
				}
			}
		} while ((*++epev).data.ptr);
	} // while (true)

	return 0;
}

int client_handling(int sock)
{
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
