#ifndef HTTP_HANDLER__
#define HTTP_HANDLER__

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define HEADER_SIZE (1024)

struct http_header {
	char *method;
	char *url;
	char *version;

	struct {
		char *length;
		char *type;
		char *encoding;
	} content;

	uint8_t *EOH; // End of header
};

void show_http_header(struct http_header *header);
void init_http_header(struct http_header *header);
int parse_http_header(char *raw, size_t size, struct http_header *header);
bool is_http_header(const char *header);

#endif
