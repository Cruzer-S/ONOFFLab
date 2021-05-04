#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#define __USE_XOPEN2K
#include <netdb.h>

#include "u_logger.h"

#define DEFAULT_PORT "1584"
#define DEFAULT_BLOG 30

int socket_reuseaddr(int sock)
{
	int sockopt = 1;

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) == -1)
		return -1;

	return 0;
}

int change_nonblocking(int fd)
{
	int pflag;

	pflag = fcntl(fd, F_GETFL);
	if (pflag == -1)
		return -1;

	if (fcntl(fd, F_SETFL, O_NONBLOCK | pflag) == -1)
		return -2;

	return 0;
}

enum make_listener_option {
	MAKE_LISTENER_BLOCKING = 0x01,
	MAKE_LISTENER_IPV6 = 0x02,
	MAKE_LISTENER_DONTREUSE = 0x04,
	MAKE_LISTENER_DGRAM = 0x08,
};

int make_listener(char *port, int backlog, 
                  struct addrinfo **ai_ret_arg,
                  int option)
{
	int sock, ret;
	struct addrinfo ai, *ai_ret;
	
	memset(&ai, 0x00, sizeof(struct addrinfo));
	ai.ai_family = (option | MAKE_LISTENER_IPV6) ? AF_INET6 : AF_INET;
	ai.ai_socktype = (option | MAKE_LISTENER_DGRAM) ? SOCK_DGRAM : SOCK_STREAM;
	ai.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;

	if ((ret = getaddrinfo(NULL, port, &ai, &ai_ret)) != 0) {
		pr_err("failed to getaddrinfo(): %s", gai_strerror(ret));
		return -1;
	}

	if ((sock = socket(ai_ret->ai_family, ai_ret->ai_socktype, ai_ret->ai_protocol)) == -1) {
		pr_err("failed to socket(): %s", strerror(errno));
		ret = -2;
		goto CLEANUP;
	}

	if (!(option | MAKE_LISTENER_DONTREUSE))
		if ((ret = socket_reuseaddr(sock)) == -1) {
			pr_err("failed to socket_reuseaddr(): %s (%d)", strerror(errno), ret);
			ret = -3;
	 		goto CLEANUP;
		}

	if (!(option | MAKE_LISTENER_BLOCKING))
		if ((ret = change_nonblocking(sock)) < 0) {
			pr_err("failed to change_nonblocking(): %s (%d)", strerror(errno), ret);
			ret = -4;
			goto CLEANUP;
		}


	if (bind(sock, ai_ret->ai_addr, ai_ret->ai_addrlen) == -1) {
		pr_err("failed to bind(): %s", strerror(errno));
		ret = -5;
		goto CLEANUP;
	}

	if (listen(sock, backlog) == -1) {
		pr_err("failed to listen(): %s", strerror(errno));
		ret = -6;
		goto CLEANUP;
	}

	if (ai_ret_arg != NULL) {
		*ai_ret_arg = ai_ret;
		return 0;
	} else ret = 0;

CLEANUP:
	freeaddrinfo(ai_ret);
	return ret;
}

int main(int argc, char *argv[])
{
	int serv_sock;
	struct addrinfo *ai;
	uint16_t backlog;
	char *port;

	if (argc == 3) {
		char *test;

		backlog = strtol(argv[2], &test, 10);
		if (test == argv[2])
			pr_crt("failed to convert backlog");
		
		argc--;
	} else backlog = DEFAULT_BLOG;

	if (argc == 2) port = argv[1];
	else           port = DEFAULT_PORT;

	serv_sock = make_listener(port, backlog, &ai, 0);
	if (serv_sock < 0)
		pr_crt("make_listener() error");

	freeaddrinfo(ai);

	close(serv_sock);

	return 0;
}
