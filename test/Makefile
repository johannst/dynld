# Copyright (c) 2020 Johannes Stoelp

check: build
	./checker

build: checker.cc
	g++ -o checker -g -O2 -I ../include -Wall -Wextra $^

clean:
	rm -f checker
