#ifndef HASH_TAB_H__
#define HASH_TAB_H__

#include <stdbool.h>

struct hashtab;

struct hashtab *hashtab_create(int bucket_size, int node_size, int (*hash_func)(void *key), int (*comp_func)(void *key1, void *key2));
void hashtab_destroy(struct hashtab *hash);
void *hashtab_find(struct hashtab *hash, void *key, bool pull_out);
int hashtab_insert(struct hashtab *hash, void *key, void *value);

#endif
