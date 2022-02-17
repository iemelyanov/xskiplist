#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct skiplist skiplist;

typedef int (*skiplistCmpFn)(const void *a, const void *b);
typedef void *(*skiplistMallocFn)(size_t);
typedef void *(*skiplistReallocFn)(void *, size_t);
typedef void (*skiplistFreeFn)(void *);

skiplist *skiplistNew(size_t element_size, skiplistCmpFn cmp);
skiplist *skiplistNewWithCustomAlloc(size_t element_size, skiplistCmpFn cmp, skiplistMallocFn malloc, skiplistReallocFn realloc, skiplistFreeFn free);

void skiplistDrop(skiplist *);
void skiplistInsert(skiplist *, void *element);
void *skiplistGet(skiplist *, void *key);
void *skiplistDel(skiplist *, void *key);
size_t skiplistLen(skiplist *);

#endif