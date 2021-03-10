#include "http_handler.h"

#define SIZEOF(X) (sizeof(X) / sizeof(X[0]))

enum HTTP_METHOD {
	GET, POST, DELETE, PETCH
};

enum HTTP_ENTITY {
	CONTENT_LENGTH, CONTENT_TYPE, CONTENT_ENCODING
};

static const char *http_method_string[] = {
	[GET]    = "GET",    [POST]  = "POST",
	[DELETE] = "DELETE", [PETCH] = "PETCH"
};

static const char *http_entity_name[] = {
	[CONTENT_LENGTH]   = "Content-Length",
	[CONTENT_TYPE]     = "Content-Type",
	[CONTENT_ENCODING] = "Content-Encoding"
};

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
