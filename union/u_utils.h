#ifndef UTILS_H__
#define UTILS_H__

#include <stdio.h>

#define EXTRACT(ptr, value) (memcpy(&value, ptr, sizeof(value)), ((ptr) + sizeof(value)))
#define LIMITS(value, max) ((value) < (max) ? (value) : (max))
#define ITOS(value, str) sprintf(str, "%d", value)
#define ASSIGN(ptr, value) (memcpy(ptr, &value, sizeof(value)), ((ptr) + sizeof(value)))
#define ASSIGN3(ptr, value, size) (memcpy(ptr, &(value), size), ((ptr) + (size)))
#define COUNTER __LINE__

#endif
