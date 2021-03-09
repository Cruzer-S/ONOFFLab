#include "server_handler.h"

#define SIZEOF(X) (sizeof(X) / sizeof(X[0]))

enum HTTP_METHOD {
	GET, POST, DELETE, PETCH
};

enum HTTP_ENTITY {
	CONTENT_LENGTH, CONTENT_TYPE, CONTENT_ENCODING
};

const char *http_method_string[] = {
	[GET]    = "GET",    [POST]  = "POST",
	[DELETE] = "DELETE", [PETCH] = "PETCH"
};

const char *http_entity_name[] = {
	[CONTENT_LENGTH]   = "Content-Length",
	[CONTENT_TYPE]     = "Content-Type",
	[CONTENT_ENCODING] = "Content-Encoding"
};

static bool is_http(char *check, struct http_header *header)
{
	char *method = strtok(check, " ");
	char *url = strtok(NULL, " ");
	char *version = strtok(NULL, " ");

	if (method == NULL || url == NULL || version == NULL)
		return false;

	for (int i = 0; i < SIZEOF(http_method_string); i++)
		if (!strcmp(method, http_method_string[i]))
			goto BREAK;
	return false;

BREAK:
	if (!strstr(version, "HTTP"))
		return false;

	header->method = method;
	header->url = url;
	header->version = version;

	return true;
}

void show_http_header(struct http_header *header)
{
	printf("Method: %s \n", header->method);
	printf("URL: %s \n", header->url);
	printf("Version: %s \n", header->version);

	printf("Content-Encoding: %s \n", header->content.encoding);
	printf("Content-Type: %s \n", header->content.type);
	printf("Content-Length: %s \n", header->content.length);
}

void init_http_header(struct http_header *header)
{
	*header = (struct http_header) { NULL, };
}

int parse_http_header(char *raw, size_t size, struct http_header *header)
{
	init_http_header(header);

	for (char *ptr = raw; (ptr - raw) < size; ptr++)
		if (*ptr == '\r' && *(ptr + 1) == '\n')
			*ptr = '\0', *(ptr + 1) = '\0';

	if (!is_http(raw, header)) {
		printf("binary data \n");
		return -1;
	}

	printf("http data \n");

	while (true) {
		while ( !(*raw == '\0' && *(raw + 1) == '\0') )
			raw++;
		raw += 2;

		if (*raw == '\0' && *(raw + 1) == '\0') {
			header->EOH = (uint8_t *)(raw + 2);
			break;
		}

		for (int i = 0; i < SIZEOF(http_entity_name); i++)
		{
			if (!strstr(raw, http_entity_name[i])) continue;
			raw += strlen(http_entity_name[i]) + 1;

			*((char **)((void *)header + sizeof(char *) * (3 + i))) = raw;
		}
	}

	return 0;
}

int register_epoll_client(int epfd, int serv_sock, int flags)
{
	int count = 0;
	while (true) {
		struct sockaddr_in clnt_adr;
		int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, (socklen_t []) { sizeof clnt_adr });

		if (clnt_sock == -1) {
			if (errno == EAGAIN) break;

			fprintf(stderr, "failed to accept client \n");
			continue;
		}

		change_flag(clnt_sock, O_NONBLOCK);

		if (register_epoll_fd(epfd, clnt_sock, EPOLLIN | EPOLLET) == -1) {
			fprintf(stderr, "register_epoll_fd(clnt_sock) error \n");
			continue;
		}

		printf("connect client: %d \n", clnt_sock);

		count++;
	}

	return count;
}

int make_server(short port, int backlog)
{
	struct sockaddr_in sock_adr;
	int sock;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
		return -1;

	memset(&sock_adr, 0, sizeof(sock_adr));
	sock_adr.sin_family = AF_INET;
	sock_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_adr.sin_port = htons(port);

	if (bind(sock, (struct sockaddr *)&sock_adr, sizeof(sock_adr)) == -1)
		return -2;

	if (listen(sock, backlog) == -1)
		return -3;

	return sock;
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
	switch (flag) {
	case SO_RCVTIMEO: ;
		struct timeval tv = {
			.tv_sec = value / 1000,
			.tv_usec = value % 1000
		};

		return setsockopt(fd, level, flag, &tv, sizeof(tv));

	default:
		return setsockopt(fd, level, flag, &value, sizeof(value));
	}

	return 0;
}

int register_epoll_fd(int epfd, int tgfd, int flag)
{
	struct epoll_event event;

	event.events = flag;
	event.data.fd = tgfd;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, tgfd, &event) == -1)
		return -1;

	return 0;
}

struct epoll_event *wait_epoll_event(int epfd, int maxevent, int timeout)
{
	static struct epoll_event events[MAX_EVENT];
	int count;

	count = epoll_wait(epfd, events, maxevent, timeout);
	if (count == -1)
		return NULL;

	events[count].data.ptr = NULL;

	return events;
}

int delete_epoll_fd(int epfd, int tgfd)
{
	int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, tgfd, NULL);

	close(tgfd);

	return ret;
}
