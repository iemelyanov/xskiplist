#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "skiplist.h"

void *(*xcalloc)(size_t, size_t) = calloc;
void (*xfree)(void *) = free;

#define MAX_LEVEL (20)

struct skiplistNode {
    int element_size;
    char data[0];
};

#define nodeElementPtr(n) ((void*)(&(n)->data[0]))
#define nodeForwards(n) ((skiplistNode**)(&(n)->data[(n)->element_size]))

struct skiplist {
    size_t element_size;
    size_t len;
    size_t level;
    skiplistCmpFn cmp;
    skiplistNode *head;
    uint32_t seed;
};

static skiplistNode *nodeNew(size_t element_size, size_t height) {
    skiplistNode *node = xcalloc(1, sizeof(skiplistNode) + element_size + sizeof(skiplistNode*) * height);
    node->element_size = element_size;
    return node;
}

static void nodeInitElement(skiplist *sk, skiplistNode *node, void *element) {
    memcpy(nodeElementPtr(node), element, sk->element_size);
}

static void nodeDrop(skiplist *sk, skiplistNode *node) {
    xfree(node);
}

skiplist *skiplistNew(size_t element_size, skiplistCmpFn cmp) {
    skiplist *sk = xcalloc(1, sizeof(*sk));
    sk->element_size = element_size;
    sk->len = 0;
    sk->level = 1;
    sk->cmp = cmp;
    sk->head = nodeNew(element_size, MAX_LEVEL);
    sk->seed = 0xdeadbeef & 0x7fffffffu;
    return sk;
}

skiplist *skiplistNewWithCustomAlloc(size_t element_size, skiplistCmpFn cmp, skiplistCallocFn calloc, skiplistFreeFn free) {
    xcalloc = calloc;
    xfree = free;
    return skiplistNew(element_size, cmp);
}

skiplist *skiplistNewWithArena(size_t element_size, skiplistCmpFn cmp) {
    return skiplistNew(element_size, cmp);
}

void skiplistDrop(skiplist *sk) {
    skiplistNode *x = sk->head;
    while (x) {
        skiplistNode *t = nodeForwards(x)[0];
        nodeDrop(sk, x);
        x = t;
    }
    xfree(sk);
}

static uint32_t randNext(skiplist *sk) {
    const uint32_t M = INT32_MAX;
    const uint64_t A = 16807; // bits 14, 8, 7, 5, 2, 1, 0
    uint64_t product = sk->seed * A;
    sk->seed = (uint32_t)((product >> 31) + (product & M));
    if (sk->seed > M) sk->seed -= M;
    return sk->seed;
}

static int randLevel(skiplist *sk) {
    int level = 1;
    const int branching = 2;
    while (level < MAX_LEVEL && (randNext(sk) % branching) == 0)
        level++;

    assert(level > 0);
    assert(level <= MAX_LEVEL);

    return level;
}

static int cmpElement(const skiplist *sk, const skiplistNode *node, void *element) {
    return sk->cmp(nodeElementPtr(node), element);
}

typedef enum {
    cmpResultEq = 0,
    cmpResultGt = 1,
} cmpResult;

static cmpResult findGtOrEq(const skiplist *sk, void *element, skiplistNode *update[static MAX_LEVEL], skiplistNode **node_out) {
    skiplistNode *x = sk->head;
    cmpResult cmp_result = cmpResultGt;

    for (int i = sk->level - 1; i >= 0; i--) {
        while (nodeForwards(x)[i] != NULL) {
            int r = cmpElement(sk, nodeForwards(x)[i], element);
            if (r < 0) {
                x = nodeForwards(x)[i];
                continue;
            }
            if (r == 0) cmp_result = cmpResultEq;
            break;
        }
        update[i] = x;
    }

    *node_out = nodeForwards(x)[0];

    return cmp_result;
}

void skiplistInsert(skiplist *sk, void *element) {
    skiplistNode *update[MAX_LEVEL] = { NULL };
    skiplistNode *x = NULL;
    cmpResult cmp_result = findGtOrEq(sk, element, update, &x);

    if (x != NULL && cmp_result == cmpResultEq) {
        nodeInitElement(sk, x, element);
        return;
    }

    int level = randLevel(sk);
    if (level > sk->level) {
        for (int i = sk->level; i < level; i++)
            update[i] = sk->head;
        sk->level = level;
    }

    x = nodeNew(sk->element_size, level);
    nodeInitElement(sk, x, element);

    for (int i = 0; i < level; i++) {
        nodeForwards(x)[i] = nodeForwards(update[i])[i];
        nodeForwards(update[i])[i] = x;
    }

    sk->len++;
}

void *skiplistGet(skiplist *sk, void *element) {
    skiplistNode *update[MAX_LEVEL] = { NULL };
    skiplistNode *x = NULL;
    cmpResult cmp_result = findGtOrEq(sk, element, update, &x);

    if (x != NULL && cmp_result == cmpResultEq) {
        return nodeElementPtr(x);
    }

    return NULL;
}

void skiplistDel(skiplist *sk, void *element) {
    skiplistNode *update[MAX_LEVEL] = { NULL };
    skiplistNode *x = NULL;
    cmpResult cmp_result = findGtOrEq(sk, element, update, &x);

    if (x != NULL && cmp_result == cmpResultEq) {
        for (int i = 0; i < sk->level; i++) {
            if (nodeForwards(update[i])[i] != x)
                break;
            nodeForwards(update[i])[i] = nodeForwards(x)[i];
        }
        nodeDrop(sk, x);
        while (sk->level > 0 && nodeForwards(sk->head)[sk->level] == NULL)
            sk->level--;
        sk->len--;
    }
}

size_t skiplistLen(skiplist *sk) {
    return sk->len;
}

void skiplistIterInit(skiplistIter *it, const skiplist *sk) {
    it->sk = sk;
    it->next = nodeForwards(it->sk->head)[0];
    it->element_size = sk->element_size;
}

bool skiplistIterNext(skiplistIter *it, void **item) {
    if (!it->next) return false;
    *item = nodeElementPtr(it->next);
    it->next = nodeForwards(it->next)[0];
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
    int64_t key;
    int64_t val;
} pair;

static int pairCmp(const void *a, const void *b) {
    const pair *pa = a;
    const pair *pb = b;
    return pa->key - pb->key;
}

int main(int argc, char **argv) {
    const int N = 1000000;
    {
        skiplist *sk = skiplistNew(sizeof(pair), &pairCmp);
        
        {
            clock_t begin = clock();
            for (int i = 0; i < N; i++) {
                skiplistInsert(sk, &(pair){.key = i, .val = i});
            }
            clock_t end = clock();
            double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
            double ns_op = elapsed_secs/(double)N*1e9;
            printf("skiplistSeqInsert:    %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
                N, elapsed_secs, ns_op, (double)N / elapsed_secs);
        }

        {
            clock_t begin = clock();
            for (int i = 0; i < N; i++) {
                pair *item = skiplistGet(sk, &(pair){.key = i});
            }
            clock_t end = clock();
            double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
            double ns_op = elapsed_secs/(double)N*1e9;
            printf("skiplistSeqGet:       %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
                N, elapsed_secs, ns_op, (double)N / elapsed_secs);
        }

        skiplistDrop(sk);
    }

    {
        skiplist *sk = skiplistNew(sizeof(pair), &pairCmp);
        
        int *rand_numbers = malloc(N * sizeof(int));
        for (int i = 0; i < N; i++)
            rand_numbers[i] = random() % N;

        {
            clock_t begin = clock();
            for (int i = 0; i < N; i++) {
                int k = rand_numbers[i];
                skiplistInsert(sk, &(pair){.key = k, .val = k});
            }
            clock_t end = clock();
            double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
            double ns_op = elapsed_secs/(double)N*1e9;
            printf("skiplistRndInsert:    %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
                N, elapsed_secs, ns_op, (double)N / elapsed_secs);
        }

        {
            clock_t begin = clock();
            for (int i = 0; i < N; i++) {
                int k = rand_numbers[i];
                pair *item = skiplistGet(sk, &(pair){.key = k});
            }
            clock_t end = clock();
            double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
            double ns_op = elapsed_secs/(double)N*1e9;
            printf("skiplistRndGet:       %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
                N, elapsed_secs, ns_op, (double)N / elapsed_secs);
        }

        free(rand_numbers);
        skiplistDrop(sk);
    }

    {
        skiplist *sk = skiplistNew(sizeof(pair), &pairCmp);

        clock_t begin = clock();
        for (int i = 0; i < N; i++) {
            skiplistInsert(sk, &(pair){.key = i, .val = i});
            pair *item = skiplistGet(sk, &(pair){.key = i});
        }
        clock_t end = clock();
        double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
        double ns_op = elapsed_secs/(double)N*1e9;
        printf("skiplistInsertAndGet: %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
            N, elapsed_secs, ns_op, (double)N / elapsed_secs);

        skiplistDrop(sk);
    }

    {
        skiplist *sk = skiplistNew(sizeof(pair), &pairCmp);
        for (int i = 0; i < N; i++) {
            skiplistInsert(sk, &(pair){.key = i, .val = i});
        }

        clock_t begin = clock();
        skiplistIter it;
        skiplistIterInit(&it, sk);
        pair *item = NULL;
        for (int i = 0; skiplistIterNext(&it, (void**)&item); i++) {
            assert(item->key == i);
            assert(item->val == i);
        }
        clock_t end = clock();
        double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
        double ns_op = elapsed_secs/(double)N*1e9;
        printf("skiplistIterNext:     %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
            N, elapsed_secs, ns_op, (double)N / elapsed_secs);

        skiplistDrop(sk);

    }
    

    return 0;
}

#endif