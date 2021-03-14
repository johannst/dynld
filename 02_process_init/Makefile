# Copyright (c) 2020 Johannes Stoelp

show: entry
	./entry 1 2 3 4

entry: entry.S entry.c
	gcc -o $@         \
	    -g -O0        \
	    -Wall -Wextra \
	    -I ../include \
	    -nostdlib     \
	    -static       \
	    $^

clean:
	rm -f entry
