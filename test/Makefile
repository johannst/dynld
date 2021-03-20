# Copyright (c) 2020 Johannes Stoelp

check: build
	./checker

build: checker.cc ../lib/libcommon.a
	g++ -o checker -g -O2 -I ../lib/include -Wall -Wextra $^

../lib/libcommon.a:
	make -C ../lib

clean:
	rm -f checker
