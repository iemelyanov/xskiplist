#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct skiplist skiplist;

typedef int (*skiplist_cmp_fn)(const void *a, const void *b);
typedef void *(*skiplist_malloc_fn)(size_t);
typedef void *(*skiplist_realloc_fn)(void *, size_t);
typedef void (*skiplist_free_fn)(void *);

struct skiplist *skiplist_new(size_t element_size, skiplist_cmp_fn cmp);
struct skiplist *skiplist_new_with_custom_alloc(size_t element_size, skiplist_cmp_fn cmp, skiplist_malloc_fn malloc, skiplist_realloc_fn realloc, skiplist_free_fn free);

void skiplist_drop(struct skiplist *);
void skiplist_insert(struct skiplist *, void *element);
void *skiplist_get(struct skiplist *, void *key);
void *skiplist_del(struct skiplist *, void *key);
size_t skiplist_len(struct skiplist *);

#endif