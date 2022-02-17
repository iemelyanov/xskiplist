build-test:
	gcc skiplist.c -std=gnu11 -O2 -fsanitize=address -D SKIPLIST_TEST -o skiplist_test

test: build-test
	./skiplist_test
