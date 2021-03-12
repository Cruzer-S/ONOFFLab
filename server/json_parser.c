#include "json_parser.h"

char *make_json(int size, ...)
{
	va_list args;
	char *buffer;

	buffer = malloc(sizeof(char) * JSON_SIZE);
	if (buffer == NULL)
		return NULL;

	va_start(args, size);

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

	return buffer;
}
