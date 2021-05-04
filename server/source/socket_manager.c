#include "socket_manager.h"

#include "logger.h"

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

int make_listener(uint16_t port, int backlog,
                  struct addrinfo **ai_ret_arg,
                  enum make_listener_option option)
{
	int sock, ret = 0;
	struct addrinfo ai, *ai_ret;
	char portstr[10];

	sprintf(portstr, "%.9d", port);
	
	memset(&ai, 0x00, sizeof(struct addrinfo));
	ai.ai_family = (option & MAKE_LISTENER_IPV6) ? AF_INET6 : AF_INET;
	ai.ai_socktype = (option & MAKE_LISTENER_DGRAM) ? SOCK_DGRAM : SOCK_STREAM;
	ai.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;

	if ((ret = getaddrinfo(NULL, portstr, &ai, &ai_ret)) != 0) {
		pr_err("failed to getaddrinfo(): %s", gai_strerror(ret));
		return -1;
	}

	if ((sock = socket(ai_ret->ai_family, ai_ret->ai_socktype, ai_ret->ai_protocol)) == -1) {
		pr_err("failed to socket(): %s", strerror(errno));
		ret = -2; goto CLEANUP;
	}

	if (!(option | MAKE_LISTENER_DONTREUSE))
		if ((ret = socket_reuseaddr(sock)) == -1) {
			pr_err("failed to socket_reuseaddr(): %s (%d)", strerror(errno), ret);
			ret = -3; goto CLEANUP;
		}

	if (!(option | MAKE_LISTENER_BLOCKING))
		if ((ret = change_nonblocking(sock)) < 0) {
			pr_err("failed to change_nonblocking(): %s (%d)", strerror(errno), ret);
			ret = -4; goto CLEANUP;
		}

	if (bind(sock, ai_ret->ai_addr, ai_ret->ai_addrlen) == -1) {
		pr_err("failed to bind(): %s", strerror(errno));
		ret = -5; goto CLEANUP;
	}

	if (listen(sock, backlog) == -1) {
		pr_err("failed to listen(): %s", strerror(errno));
		ret = -6; goto CLEANUP;
	}

	if (ai_ret_arg != NULL)
		*ai_ret_arg = ai_ret;

	return 0;

CLEANUP:
	freeaddrinfo(ai_ret);
	return ret;
}
