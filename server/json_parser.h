#ifndef JSON_PARSER_H__
#define JSON_PARSER_H__

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define JSON_SIZE 1024

void make_json(int size, char *buffer, ...);

#endif
