# SPDX-License-Identifier: MIT
#
# Copyright (c) 2020, Johannes Stoelp <dev@memzero.de>

inspect: build
	readelf -W --sections main
	readelf -W --dynamic main
	readelf -W --program-headers main
	readelf -W --string-dump .interp main
	gdb -q --batch -ex 'starti' -ex 'bt' ./main

build: libgreet.c main.c
	gcc -o libgreet.so libgreet.c -shared -fPIC
	gcc -o main main.c -lgreet -L. -Wl,--rpath=$(CURDIR)

clean:
	rm -f libgreet.so main
