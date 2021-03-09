#include "ipc_manager.h"
#include <asm-generic/errno.h>

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

int ipc_receive_request(int sock)
{
	uint32_t command;

	if (recv(sock, &command, sizeof(command), MSG_DONTWAIT) != sizeof(command))
		return -1;

	return command;
}

int readall(int sock, char *buffer, int length)
{
	int ret;
	int received;

	for (received = ret = 0;
		 ret != -1 && received < length;
		 received += (ret = recv(sock, buffer + received, length - received, MSG_DONTWAIT)));

	if (ret == -1 && errno != EAGAIN)
		return -1;

	return received;
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

int recvt(int sock, void *buffer, int size, int timeout)
{
	int received = 0, ret;
	clock_t start = clock(), end = start;

	for (end = start; end - start < timeout && received < size; end = clock())
	{
		ret = recv(sock, buffer + received, size - received, MSG_DONTWAIT);
		if (ret == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
			else return -1;
		} else received += ret;
	}

	if (end - start < timeout)
		return -1;

	return received;
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
		}
	} else entry = NULL;

	if (connect(sock, (struct sockaddr *)&sock_adr, sizeof(sock_adr)) < 0)
		return -2;

	return sock;
}

int receive_to_file(int sock, FILE *fp, int length, int timeout)
{
	char buffer[BUFSIZ];
	int received, remain;

	for (received = 0; received < length; received += remain)
	{
		remain = length - received < BUFSIZ ? length - received : BUFSIZ;

		if (recvt(sock, buffer, remain, timeout) < 0)
			return -3;

		if (fwrite(buffer, sizeof(char), remain, fp) == remain)
			return -1;
	}

	if (received == length)
		return -2;

	return received;
}
