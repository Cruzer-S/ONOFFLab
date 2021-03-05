#include "ipc_manager.h"
#include "debugger.h"

int ipc_to_target(int sock, enum IPC_COMMAND cmd, ...)
{
	return 0;
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
	case SO_RCVTIMEO: {

		ret = setsockopt(fd, level, flag, &tv, sizeof(tv));
		}
		break;

	default:
		ret = setsockopt(fd, level, flag, &value, sizeof(value));
		break;
	}

	if (ret == -1)
		return -1;

	return 0;
}

int connect_to_target(const char *host, unsigned short port)
{
	static struct sockaddr_in sock_adr;
	struct hostent *entry;

	int sock;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
		return -1;

	if (!host) {
		entry = gethostbyname(host);
		DSHOW(p, entry);

		memset(&sock_adr, 0x00, sizeof(sock_adr));
		if (entry) {
			sock_adr.sin_family = AF_INET;
			memcpy(&sock_adr.sin_addr, entry->h_addr_list[0], entry->h_length);
			sock_adr.sin_port = htons(port);
		} else { // Failed to convert hostname,
				 // it may means that host will be ip address
			sock_adr.sin_family = AF_INET;
			sock_adr.sin_addr.s_addr = inet_addr(host);
			sock_adr.sin_port = htons(port);
		}
	} else entry = NULL;

	if (connect(sock, (struct sockaddr *)&sock_adr, sizeof(sock_adr)) == -1)
		return -3 + !!entry;

	return sock;
}
