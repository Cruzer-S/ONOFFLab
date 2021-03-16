#ifndef UTILS_H__
#define UTILS_H__

#include <stdio.h>

#define EXTRACT(ptr, value) memcpy(&value, ptr, sizeof(value)), ptr += sizeof(value);
#define LIMITS(value, max) ((value) < (max) ? (value) : (max))

char *itos(int number)
{
	static char str[100];

	sprintf(str, "%d", number);

	return str;
}

#endif
