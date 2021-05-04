#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <inttypes.h>

#define __USE_XOPEN2K	// to use `struct addrinfo`
#include <netdb.h>

#include "socket_manager.h"
#include "logger.h"

#define DEFAULT_PORT 1584
#define DEFAULT_BLOG 30

int parse_arguments(int argc, char *argv[], uint16_t *port, int *backlog)
{
	char *test;
	long overflow;

	if (argc == 3) {
		overflow = strtol(argv[2], &test, 10);

		if (overflow < 0 || overflow > INT_MAX) {
			pr_err("%s", "backlog is under/overflowed");
			return -1;
		}

		if (test == argv[2]) {
			pr_err("%s", "failed to convert backlog");
			return -2;
		}

		*backlog = overflow, argc--;
	} else *backlog = DEFAULT_BLOG;

	if (argc == 2) {
		overflow = strtol(argv[1], &test, 10);
		if (test == argv[1]) {
			pr_err("%s", "failed to convert port number");
			return -3;
		}

		if (overflow < 0 && overflow > UINT16_MAX) {
			pr_err("%s", "port number is under/overflowed");
			return -4;
		}

		*port = overflow;
	} else *port = DEFAULT_PORT;

	return 0;
}

int main(int argc, char *argv[])
{
	int serv_sock;
	struct addrinfo *ai;
	int backlog, ret;
	uint16_t port;

	if (parse_arguments(argc, argv, &port, &backlog) < 0)
		pr_crt("%s", "failed to parse_arguments()");
	
	serv_sock = make_listener(port, backlog, &ai, 
			                  MAKE_LISTENER_DEFAULT);
	if (serv_sock < 0)
		pr_crt("%s", "make_listener() error");

	do {
		char host[INET6_ADDRSTRLEN];

		if ((ret = getnameinfo(
						ai->ai_addr, ai->ai_addrlen,			//address
						host, sizeof(host),						//host name
						NULL, 0,								//service name (port number)
						NI_NUMERICHOST | NI_NUMERICSERV)) != 0)	//flags
			pr_crt("failed to getnameinfo(): %s (%d)", gai_strerror(ret), ret);

		pr_out("server running at %s:%" PRIu16 " (backlog %d)", host, port, backlog);
	} while (false);
			    
	freeaddrinfo(ai);

	while (true)
	{
		// empty body 
	}

	close(serv_sock);
	
	return 0;
}


