#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

#include "logger.h"

int main(void)
{
	int servfd, rc_gai;
	struct addrinfo ai_dest, *ai_dest_ret;

	logger_create("logg.txt");

	memset(&ai_dest, 0x00, sizeof(ai_dest));
	ai_dest.ai_family = AF_UNSPEC;
	ai_dest.ai_socktype = SOCK_STREAM;
	ai_dest.ai_flags = AI_ADDRCONFIG;

	if ((rc_gai = getaddrinfo("127.0.0.1", "3606",
				  &ai_dest, &ai_dest_ret)) != 0) {
		pr_err("failed to getaddrinfo(): %s", gai_strerror(rc_gai));
		exit(EXIT_FAILURE);
	}

	servfd = socket(ai_dest_ret->ai_family,
			ai_dest_ret->ai_socktype,
			ai_dest_ret->ai_protocol);
	if (servfd == -1) {
		pr_err("failed to socket(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (connect(servfd, 
		    ai_dest_ret->ai_addr,
		    ai_dest_ret->ai_addrlen) == -1) {
		pr_err("failed to connect(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	pr_out("connected to server: %s:%s", "127.0.0.1", "3606");

	close(servfd);

	logger_destroy();

	return 0;
}
