build-test:
	gcc skiplist.c -std=gnu11 -g -fsanitize=address -D SKIPLIST_TEST -o skiplist_test

test: build-test
	./skiplist_test
