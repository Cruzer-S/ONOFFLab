#include "ipc_mananger.h"
#include "debugger.h"
#include <netinet/in.h>

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

int ipc_to_target(int sock, enum IPC_COMMAND cmd, ...)
{
	va_list args;

	va_start(args, cmd);

	switch (cmd) {
	case IPC_REGISTER_DEVICE:
		sendall
		break;
	}

	va_end(args);
}

int sendall(int sock, size_t size, const uint8_t *data)
{
	setsockopt(sock, 
}
