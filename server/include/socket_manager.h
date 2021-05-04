#ifndef SOCKET_MANAGER_H__
#define SOCKET_MANAGER_H__

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#define __USE_XOPEN2K	// to use `struct addrinfo`
#include <netdb.h>
#include <string.h>
#include <errno.h>

enum make_listener_option {
	MAKE_LISTENER_DEFAULT = 0x00,
	MAKE_LISTENER_BLOCKING = 0x01,
	MAKE_LISTENER_IPV6 = 0x02,
	MAKE_LISTENER_DONTREUSE = 0x04,
	MAKE_LISTENER_DGRAM = 0x08,
};

int socket_reuseaddr(int sock);
int change_nonblocking(int fd);
int make_listener(uint16_t port, int backlog,
                  struct addrinfo **ai_ret_arg,
                  enum make_listener_option option);

#endif
