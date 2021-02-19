#include "server_ipc.h"
#include <netdb.h>
#include <netinet/in.h>

int connect_server(const char *host, short port)
{
	int sock;
	
	struct sockaddr_in sock_adr;
	struct hostent *entry;
	
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
		return -1;

	entry = gethostbyname(host);
	if (entry == NULL)
		return -2;

	memset(&sock_adr, 0x00, sizeof(sock_adr));
	sock_adr.sin_family = AF_INET;
	sock_adr.sin_addr = *(struct in_addr*) gethostbyname(host)->h_addr_list[0];
	sock_adr.sin_port = htons(port);

	if (connect(sock, (struct sockaddr *)&sock_adr, sizeof(sock_adr)) == -1)
		return -3;

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
	int rem = 0;

	if (serv > 0) { // first-call
		if (recv(serv, &size, sizeof(uint32_t), MSG_WAITALL) == -1)
			return -1;

		if (size > max)
			rem = (size - max), size = max;
	} else { // scrape remaining data
		size = max;
	}

	if (recv(serv, data, size, MSG_WAITALL) == -1)
		return -1;

	return rem;
}
