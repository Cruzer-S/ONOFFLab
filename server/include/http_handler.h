#ifndef HTTP_HANDLER__
#define HTTP_HANDLER__

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

struct http_header {
	char *method;
	char *url;
	char *version;

	struct {
		char *length;
		char *type;
		char *encoding;
	} content;
};

enum HTTP_METHOD {
	GET, POST, PATCH, DELETE
};

void show_http_header(struct http_header *header);
void init_http_header(struct http_header *header);
int parse_http_header(char *raw, size_t size, struct http_header *header);
bool is_http_header(const char *header);
int make_http_header_s(char *receive, int recv_size, int response,
		               char *type, int length);
int parse_string_method(const char *str_method);

#endif
