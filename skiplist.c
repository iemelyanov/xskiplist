#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "skiplist.h"

void *(*xcalloc)(size_t, size_t) = calloc;
void (*xfree)(void *) = free;

#define MAX_LEVEL (20)

struct skiplist_node {
	int element_size;
	char data[0];
};

#define node_element_ptr(n) ((void*)(&(n)->data[0]))
#define node_forwards(n) ((struct skiplist_node**)(&(n)->data[(n)->element_size]))

struct skiplist {
	size_t element_size;
	size_t len;
	size_t level;
	skiplist_cmp_fn cmp;
	struct skiplist_node *head;
	uint32_t seed;
};

static struct skiplist_node *node_new(size_t element_size, size_t height)
{
	size_t size = sizeof(struct skiplist_node) + element_size + sizeof(struct skiplist_node*) * height;
	struct skiplist_node *node = xcalloc(1, size);
	node->element_size = element_size;
	return node;
}

static void node_init_element(struct skiplist *sk, struct skiplist_node *node, void *element)
{
	memcpy(node_element_ptr(node), element, sk->element_size);
}

static void node_drop(struct skiplist *sk, struct skiplist_node *node)
{
	xfree(node);
}

struct skiplist *skiplist_new(size_t element_size, skiplist_cmp_fn cmp)
{
	struct skiplist *sk = xcalloc(1, sizeof(*sk));
	sk->element_size = element_size;
	sk->len = 0;
	sk->level = 1;
	sk->cmp = cmp;
	sk->head = node_new(element_size, MAX_LEVEL);
	sk->seed = 0xdeadbeef & 0x7fffffffu;
	return sk;
}

struct skiplist *skiplist_new_with_custom_alloc(
	size_t element_size,
	skiplist_cmp_fn cmp,
	skiplist_calloc_fn calloc,
	skiplist_free_fn free)
{
	xcalloc = calloc;
	xfree = free;
	return skiplist_new(element_size, cmp);
}

void skiplist_free(struct skiplist *sk)
{
	struct skiplist_node *x = sk->head;
	while (x) {
		struct skiplist_node *t = node_forwards(x)[0];
		node_drop(sk, x);
		x = t;
	}
	xfree(sk);
}

static uint32_t rand_next(struct skiplist *sk)
{
	const uint32_t M = INT32_MAX;
	const uint64_t A = 16807; // bits 14, 8, 7, 5, 2, 1, 0
	uint64_t product = sk->seed * A;
	sk->seed = (uint32_t)((product >> 31) + (product & M));
	if (sk->seed > M) sk->seed -= M;
	return sk->seed;
}

static int rand_level(struct skiplist *sk)
{
	int level = 1;
	const int branching = 2;
	while (level < MAX_LEVEL && (rand_next(sk) % branching) == 0)
		level++;

	assert(level > 0);
	assert(level <= MAX_LEVEL);

	return level;
}

static int cmp_element(const struct skiplist *sk, const struct skiplist_node *node, void *element)
{
	return sk->cmp(node_element_ptr(node), element);
}

static int find_gt_or_eq(
	const struct skiplist *sk,
	void *element,
	struct skiplist_node *update[static MAX_LEVEL],
	struct skiplist_node **node_out)
{
	struct skiplist_node *x = sk->head;
	int cmp_result = 1;

	for (int i = sk->level - 1; i >= 0; i--) {
		while (node_forwards(x)[i] != NULL) {
			int r = cmp_element(sk, node_forwards(x)[i], element);
			if (r < 0) {
				x = node_forwards(x)[i];
				continue;
			}
			if (r == 0) cmp_result = r;
			break;
		}
		update[i] = x;
	}

	*node_out = node_forwards(x)[0];

	return cmp_result;
}

void skiplist_insert(struct skiplist *sk, void *element)
{
	struct skiplist_node *update[MAX_LEVEL] = { NULL };
	struct skiplist_node *x = NULL;
	
	int cmp_result = find_gt_or_eq(sk, element, update, &x);
	if (x != NULL && cmp_result == 0) {
		node_init_element(sk, x, element);
		return;
	}

	int level = rand_level(sk);
	if (level > sk->level) {
		for (int i = sk->level; i < level; i++)
			update[i] = sk->head;
		sk->level = level;
	}

	x = node_new(sk->element_size, level);
	node_init_element(sk, x, element);

	for (int i = 0; i < level; i++) {
		node_forwards(x)[i] = node_forwards(update[i])[i];
		node_forwards(update[i])[i] = x;
	}

	sk->len++;
}

void *skiplist_get(struct skiplist *sk, void *element)
{
	struct skiplist_node *update[MAX_LEVEL] = { NULL };
	struct skiplist_node *x = NULL;
	
	int cmp_result = find_gt_or_eq(sk, element, update, &x);
	if (x != NULL && cmp_result == 0)
		return node_element_ptr(x);

	return NULL;
}

void skiplist_del(struct skiplist *sk, void *element)
{
	struct skiplist_node *update[MAX_LEVEL] = { NULL };
	struct skiplist_node *x = NULL;
	
	int cmp_result = find_gt_or_eq(sk, element, update, &x);
	if (x != NULL && cmp_result == 0) {
		for (int i = 0; i < sk->level; i++) {
			if (node_forwards(update[i])[i] != x) break;
			node_forwards(update[i])[i] = node_forwards(x)[i];
		}
		node_drop(sk, x);
		while (sk->level > 0 && node_forwards(sk->head)[sk->level] == NULL)
			sk->level--;
		sk->len--;
	}
}

size_t skiplist_len(struct skiplist *sk)
{
	return sk->len;
}

void skiplist_iter_init(struct skiplist_iter *it, const struct skiplist *sk)
{
	it->sk = sk;
	it->next = node_forwards(it->sk->head)[0];
	it->element_size = sk->element_size;
}

bool skiplist_iter_next(struct skiplist_iter *it, void **item)
{
	if (!it->next) return false;
	*item = node_element_ptr(it->next);
	it->next = node_forwards(it->next)[0];
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

	skiplist_del(sk, &(struct pair){.key = 0});
	item = skiplist_get(sk, &(struct pair){.key = 0});
	assert(item == NULL);
	assert(skiplist_len(sk) == 9);

	skiplist_del(sk, &(struct pair){.key = 9});
	item = skiplist_get(sk, &(struct pair){.key = 9});
	assert(item == NULL);
	assert(skiplist_len(sk) == 8);

	skiplist_del(sk, &(struct pair){.key = 9});
	item = skiplist_get(sk, &(struct pair){.key = 9});
	assert(item == NULL);
	assert(skiplist_len(sk) == 8);

	for (int i = 1; i < 9; i++) {
		struct pair *item = skiplist_get(sk, &(struct pair){.key = i});
		assert(item != NULL);
		assert(item->key == i);
	}

	skiplist_del(sk, &(struct pair){.key = 5});
	item = skiplist_get(sk, &(struct pair){.key = 5});
	assert(item == NULL);
	assert(skiplist_len(sk) == 7);

	skiplist_free(sk);
	return 0;
}

#endif

#ifdef SKIPLIST_BENCH

#include <time.h>
#include <stdio.h>

struct pair {
	int64_t key;
	int64_t val;
};

static int pair_cmp(const void *a, const void *b)
{
	const struct pair *pa = a;
	const struct pair *pb = b;
	return pa->key - pb->key;
}

int main(int argc, char **argv)
{
	const int N = 1000000;
	{
		struct skiplist *sk = skiplist_new(sizeof(struct pair), &pair_cmp);
		
		{
			clock_t begin = clock();
			for (int i = 0; i < N; i++) {
				skiplist_insert(sk, &(struct pair){.key = i, .val = i});
			}
			clock_t end = clock();
			double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
			double ns_op = elapsed_secs/(double)N*1e9;
			printf("skiplist_seq_insert:     %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
				N, elapsed_secs, ns_op, (double)N / elapsed_secs);
		}

		{
			clock_t begin = clock();
			for (int i = 0; i < N; i++) {
				struct pair *item = skiplist_get(sk, &(struct pair){.key = i});
			}
			clock_t end = clock();
			double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
			double ns_op = elapsed_secs/(double)N*1e9;
			printf("skiplist_seq_get:        %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
				N, elapsed_secs, ns_op, (double)N / elapsed_secs);
		}

		skiplist_free(sk);
	}

	{
		struct skiplist *sk = skiplist_new(sizeof(struct pair), &pair_cmp);
		
		int *rand_numbers = malloc(N * sizeof(int));
		for (int i = 0; i < N; i++)
			rand_numbers[i] = random() % N;

		{
			clock_t begin = clock();
			for (int i = 0; i < N; i++) {
				int k = rand_numbers[i];
				skiplist_insert(sk, &(struct pair){.key = k, .val = k});
			}
			clock_t end = clock();
			double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
			double ns_op = elapsed_secs/(double)N*1e9;
			printf("skiplist_rnd_insert:     %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
				N, elapsed_secs, ns_op, (double)N / elapsed_secs);
		}

		{
			clock_t begin = clock();
			for (int i = 0; i < N; i++) {
				int k = rand_numbers[i];
				struct pair *item = skiplist_get(sk, &(struct pair){.key = k});
			}
			clock_t end = clock();
			double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
			double ns_op = elapsed_secs/(double)N*1e9;
			printf("skiplist_rnd_get:        %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
				N, elapsed_secs, ns_op, (double)N / elapsed_secs);
		}

		free(rand_numbers);
		skiplist_free(sk);
	}

	{
		struct skiplist *sk = skiplist_new(sizeof(struct pair), &pair_cmp);

		clock_t begin = clock();
		for (int i = 0; i < N; i++) {
			skiplist_insert(sk, &(struct pair){.key = i, .val = i});
			struct pair *item = skiplist_get(sk, &(struct pair){.key = i});
		}
		clock_t end = clock();
		double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
		double ns_op = elapsed_secs/(double)N*1e9;
		printf("skiplist_insert_and_get: %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
			N, elapsed_secs, ns_op, (double)N / elapsed_secs);

		skiplist_free(sk);
	}

	{
		struct skiplist *sk = skiplist_new(sizeof(struct pair), &pair_cmp);
		for (int i = 0; i < N; i++) {
			skiplist_insert(sk, &(struct pair){.key = i, .val = i});
		}

		clock_t begin = clock();
		struct skiplist_iter it;
		skiplist_iter_init(&it, sk);
		struct pair *item = NULL;
		for (int i = 0; skiplist_iter_next(&it, (void**)&item); i++) {
			assert(item->key == i);
			assert(item->val == i);
		}
		clock_t end = clock();
		double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC;
		double ns_op = elapsed_secs/(double)N*1e9;
		printf("skiplist_iter_next:      %d ops in %.3f secs, %.0f ns/op, %.0f op/sec\n",
			N, elapsed_secs, ns_op, (double)N / elapsed_secs);

		skiplist_free(sk);
	}

	return 0;
}

#endif