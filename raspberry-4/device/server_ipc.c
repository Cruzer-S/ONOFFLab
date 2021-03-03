#include "server_ipc.h"
#include "debugger.h"
#include <netinet/in.h>

int command_to_server(enum SERVER_IPC_COMMAND cmd, ...)
{
	int ret = 0;
	va_list args;

	va_start(args, cmd);

	switch (cmd)
	{
	case SIC_REG:
		if (send(serv, data, size, MSG_WAITALL) == -1)
			return -2;

		break;

	case SIC_REQ:
		break;

	case SIC_SYN:
		break;

	default: ret = -1;
	}

	va_end(args);

	return ret;
}

int connect_to_server(const char *host, unsigned short port)
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

int server_send_data(int serv, uint32_t size, char *data)
{
	if (send(serv, &size, sizeof(uint32_t), MSG_WAITALL) == -1)
		return -1;

	if (send(serv, data, size, MSG_WAITALL) == -1)
		return -2;

	return 0;
}

int server_recv_data(int serv, uint32_t max, char *data)
{
	uint32_t size;
	static int rem;

	if (serv >= 0) { // first-call
		rem = 0;

		// recv data size
		if (recv(serv, &size, sizeof(uint32_t), MSG_WAITALL) == -1)
			return -1;

		if (size > max)
			rem = (size - max), size = max;
	} else { // scrape remaining data
		size = max;
		rem -= (rem < size) ? rem : size;
	}

	if (recv(serv, data, size, MSG_WAITALL) == -1)
		return -1;

	return rem;
}
