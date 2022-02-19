build-test:
	gcc skiplist.c -std=gnu11 -g -fsanitize=address -D SKIPLIST_TEST -o skiplist_test

test: build-test
	./skiplist_test

build-bench:
	gcc skiplist.c -std=gnu11 -O2 -D SKIPLIST_BENCH -o skiplist_bench

bench: build-bench
	./skiplist_bench
