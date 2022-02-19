#include "skiplist.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void *(*xcalloc)(size_t, size_t) = calloc;
void (*xfree)(void *) = free;

#define MAX_LEVEL (24)

struct skiplist_node {
	struct skiplist_node *forward[MAX_LEVEL];
	void *element;
};

static struct skiplist_node *node_new(size_t element_size)
{
	struct skiplist_node *node = xcalloc(1, sizeof(*node));
	node->element = xcalloc(1, element_size);
	return node;
}

static void node_drop(struct skiplist_node *node)
{
	xfree(node->element);
	xfree(node);
}

struct skiplist {
	size_t element_size;
	size_t len;
	size_t level;
	skiplist_cmp_fn cmp;
	struct skiplist_node *head;
};

struct skiplist *skiplist_new(size_t element_size, skiplist_cmp_fn cmp)
{
	struct skiplist *sk = xcalloc(1, sizeof(*sk));
	sk->element_size = element_size;
	sk->len = 0;
	sk->level = 1;
	sk->cmp = cmp;
	sk->head = node_new(element_size);
	return sk;
}

struct skiplist *skiplist_new_with_custom_alloc(size_t element_size, skiplist_cmp_fn cmp, skiplist_calloc_fn malloc, skiplist_free_fn free)
{
	xcalloc = calloc;
	xfree = free;
	return skiplist_new(element_size, cmp);
}

void skiplist_drop(struct skiplist *sk)
{
	struct skiplist_node *x = sk->head;
	while (x) {
		struct skiplist_node *t = x->forward[0];
		node_drop(x);
		x = t;
	}
	xfree(sk);
}

static int rand_lvl()
{
	int level = 1;
	const int branching = 2;
	while (level < MAX_LEVEL && (random() % branching == 0))
		level++;
	return level;
}

void skiplist_insert(struct skiplist *sk, void *element)
{
	struct skiplist_node *update[MAX_LEVEL] = { NULL };
	struct skiplist_node *x = sk->head;

	for (int i = sk->level - 1; i >= 0; i--) {
		while (x->forward[i] != NULL && sk->cmp(x->forward[i]->element, element) < 0)
			x = x->forward[i];
		update[i] = x;
	}

	x = x->forward[0];
	if (x != NULL && sk->cmp(x->element, element) == 0) {
		memcpy(x->element, element, sk->element_size);
		return;
	}

	int level = rand_lvl();
	if (level > sk->level) {
		for (int i = sk->level; i < level; i++) {
			update[i] = sk->head;
		}
		sk->level = level;
	}

	x = node_new(sk->element_size);
	memcpy(x->element, element, sk->element_size);

	for (int i = 0; i < level; i++) {
		x->forward[i] = update[i]->forward[i];
		update[i]->forward[i] = x;
	}

	sk->len++;
}

void *skiplist_get(struct skiplist *sk, void *element)
{
	struct skiplist_node *x = sk->head;

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

void *skiplist_del(struct skiplist *l, void *key)
{
	return NULL;
}

size_t skiplist_len(struct skiplist *l)
{
	return l->len;
}

void skiplist_iter_init(struct skiplist_iter *it, const struct skiplist *sk)
{
	it->sk = sk;
	it->next = it->sk->head->forward[0];
}

bool skiplist_iter_next(struct skiplist_iter *it, void **item)
{
	if (!it->next) return false;
	*item = it->next->element;
	it->next = it->next->forward[0];
	return true;
}

#ifdef SKIPLIST_TEST

struct pair {
	int key;
	int val;
};

static int pair_cmp(const void *a, const void *b)
{
	const struct pair *pa = a;
	const struct pair *pb = b;
	return pa->key - pb->key;
}

int main(int argc, char **argv)
{
	struct skiplist *sk = skiplist_new(sizeof(struct pair), &pair_cmp);
	assert(skiplist_len(sk) == 0);
	for (int i = 0; i < 10; i++) {
		skiplist_insert(sk, &(struct pair){.key = i, .val = i});
	}
	assert(skiplist_len(sk) == 10);

	for (int i = 0; i < 10; i++) {
		struct pair *item = skiplist_get(sk, &(struct pair){.key = i});
		assert(item != NULL);
		assert(item->key == i);
		assert(item->val == i);
	}

	struct skiplist_iter it;
	skiplist_iter_init(&it, sk);
	struct pair *item = NULL;
	for (int i = 0; skiplist_iter_next(&it, (void**)&item); i++) {
		assert(item->key == i);
		assert(item->val == i);
	}

	for (int i = 0; i < 10; i++) {
		skiplist_insert(sk, &(struct pair){.key = i, .val = i+1});
	}
	assert(skiplist_len(sk) == 10);

	for (int i = 0; i < 10; i++) {
		struct pair *item = skiplist_get(sk, &(struct pair){.key = i});
		assert(item != NULL);
		assert(item->key == i);
		assert(item->val == i+1);
	}

	skiplist_drop(sk);
	return 0;
}

#endif

#ifdef SKIPLIST_BENCH

#include <time.h>
#include <stdio.h>

struct pair {
	int key;
	int val;
};

static int pair_cmp(const void *a, const void *b)
{
	const struct pair *pa = a;
	const struct pair *pb = b;
	return pa->key - pb->key;
}

int main(int argc, char **argv)
{
	struct skiplist *sk = skiplist_new(sizeof(struct pair), &pair_cmp);
	const int N = 1000000;
	
	{
		clock_t begin = clock();
		for (int i = 0; i < N; i++) {
			skiplist_insert(sk, &(struct pair){.key = i, .val = i});
		}
		clock_t end = clock();
		double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
		double ns_op = elapsed_secs/(double)N*1e9;
		printf("skiplist_insert: %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n", N, elapsed_secs, ns_op, (double)N/elapsed_secs);
	}

	{
		clock_t begin = clock();
		for (int i = 0; i < N; i++) {
			struct pair *item = skiplist_get(sk, &(struct pair){.key = i});
		}
		clock_t end = clock();
		double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
		double ns_op = elapsed_secs/(double)N*1e9;
		printf("skiplist_get:    %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n", N, elapsed_secs, ns_op, (double)N/elapsed_secs);
	}
	
	skiplist_drop(sk);
	return 0;
}

#endif