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
