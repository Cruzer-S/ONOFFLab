#include "json_parser.h"

void make_json(int size, char *buffer, ...)
{
	va_list args;

	va_start(args, buffer);

	strcpy(buffer, "{\n");
	while (size-- > 0) {
		strcat(buffer, "\t\"");
		strcat(buffer, va_arg(args, char *));
		strcat(buffer, "\": \"");
		strcat(buffer, va_arg(args, char *));
		strcat(buffer, "\"\n");
	}

	strcat(buffer, "}");

	va_end(args);
}
