# Copyright (c) 2020 Johannes Stoelp

show: entry
	./entry 1 2 3 4

entry: entry.S entry.c ../lib/libcommon.a
	gcc -o $@              \
	    -g -O0             \
	    -Wall -Wextra      \
	    -I../lib/include   \
	    -nostdlib          \
	    -static            \
	    $^

../lib/libcommon.a:
	make -C ../lib

clean:
	rm -f entry
