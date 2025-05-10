# SPDX-License-Identifier: MIT
#
# Copyright (c) 2020, Johannes Stoelp <dev@memzero.de>

show: entry
	./entry 1 2 3 4

entry: entry.S entry.c ../lib/libcommon.a
	gcc -o $@                \
	    -g -O0               \
	    -Wall -Wextra        \
	    -I../lib/include     \
	    -nostdlib            \
	    -static              \
	    -fno-stack-protector \
	    $^

../lib/libcommon.a:
	make -C ../lib

clean:
	rm -f entry
