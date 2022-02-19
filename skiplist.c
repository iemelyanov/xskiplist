#include "skiplist.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *(*xmalloc)(size_t) = malloc;
void *(*xrealloc)(void *, size_t) = realloc;
void (*xfree)(void *) = free;

#define MAX_LEVEL (24)

struct node {
	struct node *forward[MAX_LEVEL];
	void *element;
};

static struct node *node_new(size_t element_size)
{
	struct node *node = xmalloc(sizeof(*node));
	memset(node, 0, sizeof(*node));
	node->element = xmalloc(element_size);
	return node;
}

static void node_drop(struct node *node)
{
	xfree(node->element);
	xfree(node);
}

struct skiplist {
	size_t element_size;
	size_t len;
	size_t level;
	skiplist_cmp_fn cmp;
	struct node *head;
};

struct skiplist *skiplist_new(size_t element_size, skiplist_cmp_fn cmp)
{
	struct skiplist *sk = xmalloc(sizeof(*sk));
	sk->element_size = element_size;
	sk->len = 0;
	sk->level = 1;
	sk->cmp = cmp;
	sk->head = node_new(element_size);
	return sk;
}

struct skiplist *skiplist_new_with_custom_alloc(size_t element_size, skiplist_cmp_fn cmp, skiplist_malloc_fn malloc,
	skiplist_realloc_fn realloc, skiplist_free_fn free)
{
	xmalloc = malloc;
	xrealloc = realloc;
	xfree = free;
	return skiplist_new(element_size, cmp);
}

void skiplist_drop(struct skiplist *sk)
{
	struct node *x = sk->head;
	while (x) {
		struct node *t = x->forward[0];
		node_drop(x);
		x = t;
	}
	xfree(sk);
}

static int rand_lvl()
{
	int level = 1;
	int branching = 2;
	while (level < MAX_LEVEL && (random() % branching == 0))
		level++;
	return level;
}

void skiplist_insert(struct skiplist *sk, void *element)
{
	struct node *update[MAX_LEVEL] = { NULL };
	struct node *x = sk->head;

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
	struct node *x = sk->head;

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