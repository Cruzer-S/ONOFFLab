#include "server_ipc.h"

int connect_server(const char *host, short port)
{
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in sock_adr;

	if (sock == -1)
		return -1;

	memset(&sock_adr, 0x00, sizeof(sock_adr));
	sock_adr.sin_family = AF_INET;
	sock_adr.sin_addr.s_addr = inet_addr(host);
	sock_adr.sin_port = htons(port);

	if (connect(sock, (struct sockaddr *)&sock_adr, sizeof(sock_adr)) == -1)
		error_handling("connect() error \n");

	return sock;
}

int send_server(int serv, uint32_t size, char *data)
{
	if (send(serv, &size, sizeof(uint32_t), 0) == -1)
		return -1;

	if (send(serv, data, size, 0) == -1)
		return -1;

	return 0;
}
