#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct skiplist;
struct skiplist_node;

struct skiplist_iter {
	const struct skiplist *sk;
	struct skiplist_node *next;
};

typedef int (*skiplist_cmp_fn)(const void *a, const void *b);
typedef void *(*skiplist_calloc_fn)(size_t nmemb, size_t size);
typedef void (*skiplist_free_fn)(void *);

struct skiplist *skiplist_new(size_t element_size, skiplist_cmp_fn cmp);
struct skiplist *skiplist_new_with_custom_alloc(size_t element_size, skiplist_cmp_fn cmp, skiplist_calloc_fn calloc, skiplist_free_fn free);

void skiplist_drop(struct skiplist *sk);
void skiplist_insert(struct skiplist *sk, void *element);
void *skiplist_get(struct skiplist *sk, void *key);
void *skiplist_del(struct skiplist *sk, void *key);
size_t skiplist_len(struct skiplist *sk);

void skiplist_iter_init(struct skiplist_iter *it, const struct skiplist *sk);
bool skiplist_iter_next(struct skiplist_iter *it, void **item);

#endif