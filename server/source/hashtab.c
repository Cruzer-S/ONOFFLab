#include "hashtab.h"

#include "logger.h"

#include <stdio.h>	// NULL
#include <errno.h>	// errno
#include <string.h>	// strerror

struct node {
	void *key;
	void *value;
	struct node *next;
};

struct hashtab {
	int (*hash_func)(void *key);
	int (*comp_func)(void *k1, void *k2);

	int bucket_size;
	int freed_node_count;
	int used_node_count;

	struct node *bucket;

	struct node *freed_node;
	struct node *freed_node_origin;
};

void *hashtab_find(Hashtab Hash, void *key, bool pull_out)
{
	int index;
	struct node *bucket, *prev, *cur;
	struct hashtab *hash = Hash;

	index = hash->hash_func(key) % hash->bucket_size;

	bucket = &hash->bucket[index];

	for (prev = cur = bucket->next;
		 cur != NULL;
		 prev = cur, cur = cur->next)
	{
		if (hash->comp_func(cur->key, key) == 0) {
			if (pull_out) {
				hash->freed_node_count++;

				if (prev->next)
					prev->next = cur->next;
				else
					bucket->next = NULL;

				cur->next = hash->freed_node;
				hash->freed_node = cur;
			}

			return cur->value;
		}
	}

	return NULL;
}

int hashtab_insert(Hashtab Hash, void *key, void *value)
{
	struct node *bucket, *new_node;
	struct hashtab *hash = Hash;
	int index;
	void *ptr;

	if ((ptr = hashtab_find(hash, key, false))) {
		pr_err("find the same key in the hashtab: %p", ptr);
		return -1;
	}

	if (hash->freed_node <= 0) {
		pr_err("failed to allocate hash: %s", "freed node is zero");
		return -2;
	} else {	
		new_node = hash->freed_node;
		hash->freed_node = hash->freed_node->next;
		hash->freed_node_count--;
	}

	new_node->key = key;
	new_node->value = value;

	index = hash->hash_func(key) % hash->bucket_size;
	bucket = &hash->bucket[index];
	
	if (bucket->next)
		new_node->next = bucket->next;
	else
		new_node->next = NULL;

	bucket->next = new_node;
	
	return 0;
}

Hashtab hashtab_create(
		int bucket_size,
		int node_size,
		int (*hash_func)(void *key),
		int (*comp_func)(void *key1, void *key2))
{
	struct hashtab *hash;

	hash = malloc(sizeof(struct hashtab));
	if (hash == NULL) {
		pr_err("failed to malloc(struct hashtab): %s", strerror(errno));
		return NULL;
	}

	hash->hash_func = hash_func;
	hash->comp_func = comp_func;
	hash->bucket_size = bucket_size;

	hash->freed_node_count = node_size;
	hash->used_node_count = 0;

	hash->freed_node = NULL;

	hash->bucket = malloc(sizeof(struct node) * hash->bucket_size);
	if (hash->bucket == NULL) {
		pr_err("failed to malloc(struct node: bucket): %s", strerror(errno));
		free(hash);
		return NULL;
	} else for (int i = 0; i < hash->bucket_size; i++) {
		hash->bucket[i].next = NULL;
	}

	struct node *freed_node = malloc(
		sizeof(struct node) * hash->freed_node_count);
	if (freed_node == NULL) {
		pr_err("failed to malloc(struct node: freed node): %s", strerror(errno));
		free(hash->bucket);
		free(hash);
		return NULL;
	}

	for (int i = 0; i < hash->freed_node_count - 1; i++)
		freed_node[i].next = &freed_node[i + 1];
	freed_node[hash->freed_node_count].next = NULL;

	hash->freed_node_origin = hash->freed_node = freed_node;

	return hash;
}

void hashtab_destroy(Hashtab Hash)
{
	struct hashtab *hash = Hash;
	struct node *tmp;

	while (hash->freed_node) {
		tmp = hash->freed_node->next;
		free(hash->freed_node);
		hash->freed_node = tmp;
	}

	free(hash->bucket);
	free(hash);
}
