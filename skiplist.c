#include "skiplist.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *(*xmalloc)(size_t) = malloc;
void *(*xrealloc)(void *, size_t) = realloc;
void (*xfree)(void *) = free;

#define MAX_LEVEL (24)

typedef struct node node;

struct node {
    node *forward[MAX_LEVEL];
    void *element;
};

static node *nodeNew(size_t element_size) {
    node *node = xmalloc(sizeof(*node));
    memset(node, 0, sizeof(*node));
    node->element = xmalloc(element_size);
    return node;
}

static void nodeDrop(node *node) {
    xfree(node->element);
    xfree(node);
}

struct skiplist {
    size_t element_size;
    size_t size;
    size_t level;
    skiplistCmpFn cmp;
    node *head;
};

skiplist *skiplistNew(size_t element_size, skiplistCmpFn cmp) {
    skiplist *sk = xmalloc(sizeof(*sk));
    sk->element_size = element_size;
    sk->size = 0;
    sk->level = 1;
    sk->cmp = cmp;
    sk->head = nodeNew(element_size);
    return sk;
}

skiplist *skiplistNewWithCustomAlloc(size_t element_size, skiplistCmpFn cmp, skiplistMallocFn malloc, skiplistReallocFn realloc, skiplistFreeFn free) {
    xmalloc = malloc;
    xrealloc = realloc;
    xfree = free;
    return skiplistNew(element_size, cmp);
}

void skiplistDrop(skiplist *sk) {
    node *x = sk->head;
    while (x) {
        node *t = x->forward[0];
        nodeDrop(x);
        x = t;
    }
    xfree(sk);
}

static int randLevel() {
    int level = 1;
    int branching = 2;
    while (level < MAX_LEVEL && (random() % branching == 0))
        level++;
    return level;
}

void skiplistInsert(skiplist *sk, void *element) {
    node *update[MAX_LEVEL] = {NULL};
    node *x = sk->head;

    for (int i = sk->level - 1; i >= 0; i--) {
        while (x->forward[i] != NULL && sk->cmp(x->forward[i]->element, element) < 0)
            x = x->forward[i];
        update[i] = x;
    }

    x = x->forward[0];
    if (x != NULL && sk->cmp(x->element, element) == 0) {
        assert(x->element != NULL);
        xfree(x->element);
        x->element = element;
        return;
    }

    int level = randLevel();
    if (level > sk->level) {
        for (int i = sk->level; i < level; i++) {
            update[i] = sk->head;
        }
        sk->level = level;
    }

    x = nodeNew(sk->element_size);
    memcpy(x->element, element, sk->element_size);

    for (int i = 0; i < level; i++) {
        x->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = x;
    }

    sk->size++;
}

void *skiplistGet(skiplist *sk, void *element) {
    node *x = sk->head;

    for (int i = sk->level - 1; i >= 0; i--) {
        while (x->forward[i] != NULL && sk->cmp(x->forward[i]->element, element) < 0) {
            x = x->forward[i];
        }
    }

    x = x->forward[0];
    if (x != NULL && sk->cmp(x->element, element) == 0) {
        assert(x->element != NULL);
        return x->element;
    }

    return NULL;
}

void *skiplistDel(skiplist *l, void *key) {
    return NULL;
}

size_t skiplistLen(skiplist *l) {
    return 0;
}

#ifdef SKIPLIST_TEST

typedef struct pair {
    int key;
    int val;
} pair;

static int pairCmp(const void *a, const void *b) {
    const pair *pa = a;
    const pair *pb = b;
    return pa->key - pb->key;
}

int main(int argc, char **argv) {
    skiplist *sk = skiplistNew(sizeof(pair), &pairCmp);
    for (int i = 0; i < 1; i++) {
        skiplistInsert(sk, &(pair){.key = i, .val = i});
    }
    for (int i = 0; i < 1; i++) {
        pair *item = skiplistGet(sk, &(pair){.key = i});
        assert(item != NULL);
        assert(item->key == i);
        assert(item->val == i);
    }
    skiplistDrop(sk);
    return 0;
}

#endif