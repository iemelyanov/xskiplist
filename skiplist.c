#include "skiplist.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void *(*xcalloc)(size_t, size_t) = calloc;
void (*xfree)(void *) = free;

#define MAX_LEVEL (24)
#define SMALL_ELEMENT_SIZE (16)

struct skiplistNode {
    skiplistNode *forward[MAX_LEVEL];
    union {
        void *element;
        char small_element[SMALL_ELEMENT_SIZE];
    };
};

struct skiplist {
    size_t element_size;
    size_t len;
    size_t level;
    skiplistCmpFn cmp;
    skiplistNode *head;
};

static skiplistNode *nodeNew(size_t element_size) {
    return xcalloc(1, sizeof(skiplistNode));
}

static void nodeInitElement(skiplist *sk, skiplistNode *node, void *element) {
    if (sk->element_size > SMALL_ELEMENT_SIZE) {
        if (node->element == NULL)
            node->element = xcalloc(1, sk->element_size);
        memcpy(node->element, element, sk->element_size);
        return;
    }
    memcpy(node->small_element, element, sk->element_size);
}

static void nodeDrop(skiplist *sk, skiplistNode *node) {
    if (sk->element_size > SMALL_ELEMENT_SIZE) {
        xfree(node->element);
        node->element = NULL;
    }
    xfree(node);
}

skiplist *skiplistNew(size_t element_size, skiplistCmpFn cmp) {
    skiplist *sk = xcalloc(1, sizeof(*sk));
    sk->element_size = element_size;
    sk->len = 0;
    sk->level = 1;
    sk->cmp = cmp;
    sk->head = nodeNew(element_size);
    return sk;
}

skiplist *skiplistNewWithCustomAlloc(size_t element_size, skiplistCmpFn cmp, skiplistCallocFn malloc, skiplistFreeFn free) {
    xcalloc = calloc;
    xfree = free;
    return skiplistNew(element_size, cmp);
}

void skiplistDrop(skiplist *sk) {
    skiplistNode *x = sk->head;
    while (x) {
        skiplistNode *t = x->forward[0];
        nodeDrop(sk, x);
        x = t;
    }
    xfree(sk);
}

static int randLevel() {
    int level = 1;
    const int branching = 2;
    while (level < MAX_LEVEL && (random() % branching == 0))
        level++;
    return level;
}

static int cmpElement(const skiplist *sk, const skiplistNode *node, void *element) {
    if (sk->element_size > SMALL_ELEMENT_SIZE)
        return sk->cmp(node->element, element);
    return sk->cmp(node->small_element, element);
}

static skiplistNode *findGtOrEq(const skiplist *sk, void *element, skiplistNode *update[static MAX_LEVEL]) {
    skiplistNode *x = sk->head;

    for (int i = sk->level - 1; i >= 0; i--) {
        while (x->forward[i] != NULL && cmpElement(sk, x->forward[i], element) < 0)
            x = x->forward[i];
        update[i] = x;
    }

    return x->forward[0];
}

void skiplistInsert(skiplist *sk, void *element) {
    skiplistNode *update[MAX_LEVEL] = { NULL };
    skiplistNode *x = findGtOrEq(sk, element, update);

    if (x != NULL && cmpElement(sk, x, element) == 0) {
        nodeInitElement(sk, x, element);
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
    nodeInitElement(sk, x, element);

    for (int i = 0; i < level; i++) {
        x->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = x;
    }

    sk->len++;
}

void *skiplistGet(skiplist *sk, void *element) {
    skiplistNode *update[MAX_LEVEL] = { NULL };
    skiplistNode *x = findGtOrEq(sk, element, update);

    if (x != NULL && cmpElement(sk, x, element) == 0) {
        return sk->element_size > SMALL_ELEMENT_SIZE ? x->element : &x->small_element[0];
    }

    return NULL;
}

void skiplistDel(skiplist *sk, void *element) {
    skiplistNode *update[MAX_LEVEL] = { NULL };
    skiplistNode *x = findGtOrEq(sk, element, update);

    if (x != NULL && cmpElement(sk, x, element) == 0) {
        for (int i = 0; i < sk->level; i++) {
            if (update[i]->forward[i] != x)
                break;
            update[i]->forward[i] = x->forward[i];
        }
        nodeDrop(sk, x);
        while (sk->level > 0 && sk->head->forward[sk->level] == NULL)
            sk->level--;
        sk->len--;
    }
}

size_t skiplistLen(skiplist *sk) {
    return sk->len;
}

void skiplistIterInit(skiplistIter *it, const skiplist *sk) {
    it->sk = sk;
    it->next = it->sk->head->forward[0];
    it->element_size = sk->element_size;
}

bool skiplistIterNext(skiplistIter *it, void **item) {
    if (!it->next) return false;
    *item = it->element_size > SMALL_ELEMENT_SIZE ?  it->next->element : &it->next->small_element[0];
    it->next = it->next->forward[0];
    return true;
}

#ifdef SKIPLIST_TEST

typedef struct {
    int key;
    int val;
} pair;

static int pairCmp(const void *a, const void *b) {
    const pair *pa = a;
    const pair *pb = b;
    return pa->key - pb->key;
}

int main(int argc, char **argv) {
    struct skiplist *sk = skiplistNew(sizeof(pair), &pairCmp);
    assert(skiplistLen(sk) == 0);
    for (int i = 0; i < 10; i++) {
        skiplistInsert(sk, &(pair){.key = i, .val = i});
    }
    assert(skiplistLen(sk) == 10);

    for (int i = 0; i < 10; i++) {
        pair *item = skiplistGet(sk, &(pair){.key = i});
        assert(item != NULL);
        assert(item->key == i);
        assert(item->val == i);
    }

    skiplistIter it;
    skiplistIterInit(&it, sk);
    pair *item = NULL;
    for (int i = 0; skiplistIterNext(&it, (void**)&item); i++) {
        assert(item->key == i);
        assert(item->val == i);
    }

    for (int i = 0; i < 10; i++) {
        skiplistInsert(sk, &(pair){.key = i, .val = i+1});
    }
    assert(skiplistLen(sk) == 10);

    for (int i = 0; i < 10; i++) {
        pair *item = skiplistGet(sk, &(pair){.key = i});
        assert(item != NULL);
        assert(item->key == i);
        assert(item->val == i+1);
    }

    skiplistDel(sk, &(pair){.key = 0});
    item = skiplistGet(sk, &(pair){.key = 0});
    assert(item == NULL);
    assert(skiplistLen(sk) == 9);

    skiplistDel(sk, &(pair){.key = 9});
    item = skiplistGet(sk, &(pair){.key = 9});
    assert(item == NULL);
    assert(skiplistLen(sk) == 8);

    skiplistDel(sk, &(pair){.key = 9});
    item = skiplistGet(sk, &(pair){.key = 9});
    assert(item == NULL);
    assert(skiplistLen(sk) == 8);

    for (int i = 1; i < 9; i++) {
        pair *item = skiplistGet(sk, &(pair){.key = i});
        assert(item != NULL);
        assert(item->key == i);
    }

    skiplistDel(sk, &(pair){.key = 5});
    item = skiplistGet(sk, &(pair){.key = 5});
    assert(item == NULL);
    assert(skiplistLen(sk) == 7);

    skiplistDrop(sk);
    return 0;
}

#endif

#ifdef SKIPLIST_BENCH

#include <time.h>
#include <stdio.h>

typedef struct {
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
    const int N = 1000000;
    
    {
        clock_t begin = clock();
        for (int i = 0; i < N; i++) {
            skiplistInsert(sk, &(pair){.key = i, .val = i});
        }
        clock_t end = clock();
        double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
        double ns_op = elapsed_secs/(double)N*1e9;
        printf("skiplistInsert: %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n", N, elapsed_secs, ns_op, (double)N/elapsed_secs);
    }

    {
        clock_t begin = clock();
        for (int i = 0; i < N; i++) {
            pair *item = skiplistGet(sk, &(pair){.key = i});
        }
        clock_t end = clock();
        double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
        double ns_op = elapsed_secs/(double)N*1e9;
        printf("skiplistGet:    %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n", N, elapsed_secs, ns_op, (double)N/elapsed_secs);
    }
    
    skiplistDrop(sk);
    return 0;
}

#endif