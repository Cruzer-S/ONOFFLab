#include "ipc_manager.h"

#define ASSIGN(ptr, value) memcpy(ptr, &value, sizeof(value)), ptr += sizeof(value)

int ipc_to_target(int sock, enum IPC_COMMAND cmd, ...)
{
	va_list args;
	uint8_t header[HEADER_SIZE], *hp = header;

	va_start(args, cmd);

	do { // Extract command and assign to header
		uint32_t command = (uint32_t) cmd;
		hp = ASSIGN(hp, command);
	} while (false);

	switch (cmd) {
	case IPC_REGISTER_DEVICE: ;
		uint32_t dev_id = va_arg(args, uint32_t);
		hp = ASSIGN(hp, dev_id);
		break;

	default: break;
	}

	if (send(sock, header, sizeof(header), 0) != sizeof(header))
		return -1;

	va_end(args);

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
	case SO_RCVTIMEO: ;
		struct timeval tv = {
			.tv_sec = value / 1000,
			.tv_usec = value % 1000
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

int connect_to_target(const char *host, uint16_t port)
{
	static struct sockaddr_in sock_adr;
	struct hostent *entry;

	int sock;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
		return -1;

	if (host) {
		entry = gethostbyname(host);

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

	if (connect(sock, (struct sockaddr *)&sock_adr, sizeof(sock_adr)) < 0)
		return -2;

	return sock;
}
