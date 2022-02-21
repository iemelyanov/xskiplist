#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct skiplist skiplist;
typedef struct skiplistNode skiplistNode;

typedef struct {
    const skiplist *sk;
    skiplistNode *next;
    size_t element_size;
} skiplistIter;

typedef int (*skiplistCmpFn)(const void *a, const void *b);
typedef void *(*skiplistCallocFn)(size_t nmemb, size_t size);
typedef void (*skiplistFreeFn)(void *);

struct skiplist *skiplistNew(size_t element_size, skiplistCmpFn cmp);
struct skiplist *skiplistNewWithCustomAlloc(size_t element_size, skiplistCmpFn cmp, skiplistCallocFn calloc, skiplistFreeFn free);

void skiplistDrop(skiplist *sk);
void skiplistInsert(skiplist *sk, void *element);
void *skiplistGet(skiplist *sk, void *key);
void skiplistDel(skiplist *sk, void *key);
size_t skiplistLen(skiplist *sk);

void skiplistIterInit(skiplistIter *it, const struct skiplist *sk);
bool skiplistIterNext(skiplistIter *it, void **item);

#endif