#ifndef HASH_TAB_H__
#define HASH_TAB_H__

#include <stdbool.h>

typedef void *Hashtab;

Hashtab hashtab_create(
		int bucket_size, int node_size,
		int (*hash_func)(void *key),
		int (*comp_func)(void *key1, void *key2));
void hashtab_destroy(Hashtab hash);
void *hashtab_find(Hashtab hash, void *key, bool pull_out);
int hashtab_insert(Hashtab hash, void *key, void *value);

#endif
