#include "ipc_manager.h"
#include <asm-generic/errno.h>

#define ASSIGN(ptr, value) memcpy(ptr, &value, sizeof(value)), ptr += sizeof(value)

int ipc_to_target(int sock, enum IPC_COMMAND cmd, ...)
{
	va_list args;
	uint8_t header[HEADER_SIZE], *hp = header;

	va_start(args, cmd);

	do { // Extract command and assign to header
		int32_t command = (int32_t) cmd;
		hp = ASSIGN(hp, command);
	} while (false);

	switch (cmd) {
	case IPC_REGISTER_DEVICE: ;
		int32_t dev_id = va_arg(args, int32_t);
		hp = ASSIGN(hp, dev_id);
		break;

	default: break;
	}

	if (sendt(sock, header, sizeof(header), CPS) < 0)
		return -1;

	do {
		int32_t result;

		if (recvt(sock, &result, sizeof(result), CPS) < 0)
			return -2;

		if (result < 0)
			return -(3 + result);
	} while (false);

	va_end(args);

	return 0;
}

int ipc_receive_request(int sock)
{
	int32_t command;

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

int flush_socket(int sock)
{
	char buffer[BUFSIZ];

	while (recv(sock, buffer, sizeof(buffer), MSG_DONTWAIT) != -1)
		/* empty loop body */ ;

	return -(errno == EAGAIN);
}

int recvt(int sock, void *buffer, int size, clock_t timeout)
{
	int received = 0, ret;
	clock_t start = clock(), end = start;

	for (end = start; end - start < timeout && received < size; end = clock())
	{
		ret = recv(sock, buffer + received, size - received, MSG_DONTWAIT);
		if (ret == 0) return -2;

		if (ret == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
			else return -1;
		} else received += ret;
	}

	return received;
}

int sendt(int sock, void *buffer, int size, clock_t timeout)
{
	int to_send = 0, ret;
	clock_t start = clock(), end = start;

	for (end = start; end - start < timeout && to_send < size; end = clock())
	{
		ret = send(sock, buffer + to_send, size - to_send, MSG_DONTWAIT);
		if (ret == 0) return -3;

		if (ret == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
			else return -1;
		} else to_send += ret;
	}

	return to_send;
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

		sock_adr.sin_family = AF_INET;
		sock_adr.sin_port = htons(port);

		if (entry) {
			memcpy(&sock_adr.sin_addr, entry->h_addr_list[0], entry->h_length);
		} else { // Failed to convert hostname,
				 // it may means that host will be ip address
			sock_adr.sin_addr.s_addr = inet_addr(host);
		}
	}

	if (connect(sock, (struct sockaddr *)&sock_adr, sizeof(sock_adr)) < 0) {
		close(sock);
		return -2;
	}

	return sock;
}
