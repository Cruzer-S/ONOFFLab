#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "server_handler.h"

_Noreturn void error_handling(const char *format, ...);

int client_handler(int clnt)
{
	char buffer[BUFSIZ];
	uint32_t size;
	int ret;

	if (recv(clnt, &size, sizeof(uint32_t), MSG_WAITALL) < 0)
		return false;

	printf("Client %d said that: ", clnt);
	while (size > 0) {
		if (size >= BUFSIZ)
			size -= (ret = recv(clnt, buffer, BUFSIZ - 1, 0));
		else
			size -= (ret = recv(clnt, buffer, size, 0));

		if (ret == -1)
			return false;

		buffer[ret] = '\0';

		fputs(buffer, stdout);
	}
	fputc('\n', stdout);

	return true;
}

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock, epfd;
	int ret;
	struct sockaddr_in clnt_adr;

	if (argc != 3)
		error_handling("usage: <%s> <port> <backlog> \n", argv[0]);	

	if ((serv_sock = make_server(strtol(argv[1], NULL, 10), 
								 strtol(argv[2], NULL, 10))) < 0)
		error_handling("failed to create server: %d \n", serv_sock);

	if ((ret = change_flag(serv_sock, O_NONBLOCK)) < 0)
		error_handling("change_flag(): %d \n", ret);

	if (change_sockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, true) == -1)
		error_handling("change_sockopt() error \n");
	
	epfd = epoll_create(1024);
	if (epfd == -1)
		error_handling("epoll_create() error \n");

	if (register_epoll_fd(epfd, serv_sock, EPOLLIN | EPOLLET) == -1)
		error_handling("register_epoll_fd() error \n");

	while (true)
	{
		struct epoll_event *epev = wait_epoll_event(epfd, MAX_EVENT, 5000);
		if (epev == NULL)
			error_handling("wait_epoll_event() error \n");

		if (epev->data.ptr == NULL) { // time-out
			printf("time-out\n");
			continue;
		}

		do {
			if (epev->data.fd == serv_sock) {
				int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, (socklen_t []) { 0 });
				if (clnt_sock == -1) {
					fprintf(stderr, "failed to accept client \n");
					continue;
				}

				/*
				if ((ret = change_flag(clnt_sock, O_NONBLOCK)) == -1) {
					fprintf(stderr, "change_flag(clnt_sock) error: %d \n", ret);
					continue;
				}
				*/
				if (change_sockopt(epev->data.fd, SOL_SOCKET, SO_RCVTIMEO, 5000))
					fprintf(stderr, "failed to set client socket option \n");

				if (register_epoll_fd(epfd, clnt_sock, EPOLLIN | EPOLLET) == -1) {
					fprintf(stderr, "register_epoll_fd(clnt_sock) error \n");
					continue;
				}

				printf("connect client: %d \n", clnt_sock);
			} else if (!client_handler(epev->data.fd)) {
				printf("disconnected client: %d \n", epev->data.fd);
				epoll_ctl(epfd, EPOLL_CTL_DEL, epev->data.fd, NULL); 
			}
		} while ((*++epev).data.ptr);
	}

	close(epfd);
	close(serv_sock);

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
