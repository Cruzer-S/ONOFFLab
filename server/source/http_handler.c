#include "http_handler.h"

#define SIZEOF(X) (sizeof(X) / sizeof(X[0]))

enum HTTP_ENTITY {
	CONTENT_LENGTH, CONTENT_TYPE, CONTENT_ENCODING
};

static const char *http_method_string[] = {
	[GET]    = "GET",    [POST]  = "POST",
	[PATCH]  = "PATCH", [DELETE] = "DELTE"
};

static const char *http_entity_name[] = {
	[CONTENT_LENGTH]   = "Content-Length",
	[CONTENT_TYPE]     = "Content-Type",
	[CONTENT_ENCODING] = "Content-Encoding"
};

int parse_string_method(const char *str_method)
{
	for (int i = 0; i < SIZEOF(http_method_string); i++)
		if (!strcmp(str_method, http_method_string[i]))
			return i;

	return -1;
}

bool is_http_header(const char *header)
{
	if (strstr(header, "\r\n\r\n") && strstr(header, "HTTP"))
		return true;

	return false;
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

	header->method  = strtok(raw, " ");
	header->url     = strtok(NULL, " ");
	header->version = strtok(NULL, "\r");

	raw = header->version + 2;
	while (true) {
		for (int i = 0; i < SIZEOF(http_entity_name); i++)
		{
			if (strncmp(raw, http_entity_name[i], strlen(http_entity_name[i]))) continue;
			raw += strlen(http_entity_name[i]) + 2;

			*((char **)((void *)header + sizeof(char *) * (3 + i))) = raw;
		}

		while ( !(*raw == '\r' && *(raw + 1) == '\n') ) raw++;
		*raw = '\0', raw += 2;
		if (*raw == '\r' && *(raw + 1) == '\n') break;
	}

	return 0;
}

static char *ctos(int code)
{
	switch (code) {
	case 200: return "OK";
	case 404: return "Not Found";
	}

	return NULL;
}

int make_http_header_s(char *receive, int recv_size, int response,
		               char *type, int length)
{
	return snprintf(receive, recv_size,
		    "HTTP/1.1 %d %s\r\n"
			"Content-type: %s\r\n"
			"Content-length: %d\r\n"
			"\r\n",
			response, ctos(response),
			type, length
	);
}
