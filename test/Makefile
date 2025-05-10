# SPDX-License-Identifier: MIT
#
# Copyright (c) 2020, Johannes Stoelp <dev@memzero.de>

check: build
	./checker

build: checker.cc test_helper.h ../lib/libcommon.a
	g++ -o checker                  \
	    -g -O2                      \
	    -I ../lib/include           \
	    -Wall -Wextra               \
	    -fsanitize=address          \
	    -fsanitize=pointer-compare  \
	    -fsanitize=pointer-subtract \
	    -fsanitize=undefined        \
	    $(filter-out %.h, $^)

../lib/libcommon.a:
	make -C ../lib

clean:
	rm -f checker
	make -C ../lib clean
