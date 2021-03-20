# Copyright (c) 2021 Johannes Stoelp

COMMON_CFLAGS := -g -O0 -Wall -Wextra \
                 -I../lib/include -nostdlib

run: main
	./$<

main: dynld.so main.c ../lib/libcommon.a
	gcc -o $@                                   \
	    $(COMMON_CFLAGS)                        \
	    -Wl,--dynamic-linker=$(CURDIR)/dynld.so \
	    $(filter %.c %.a, $^)

	readelf -W --dynamic $@
	readelf -W --string-dump .interp $@
	readelf -W --program-headers $@

dynld.so: dynld.S dynld.c ../lib/libcommon.a
	gcc -o $@                          \
	    $(COMMON_CFLAGS)               \
	    -fPIC                          \
	    -fvisibility=hidden            \
	    -Wl,--entry=dl_start           \
	    -Wl,--no-allow-shlib-undefined \
	    $^

../lib/libcommon.a:
	make -C ../lib

clean:
	rm -f main
	rm -f dynld.so
