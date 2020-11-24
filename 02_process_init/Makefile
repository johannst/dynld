# Copyright (c) 2020 Johannes Stoelp

show: entry
	#gdb -q --batch -ex 'starti' -ex 'x/g $$rsp' -ex 'x/s *(char**)($$rsp+8)' ./$^
	./entry 1 2 3 4

entry: entry.S entry.c
	gcc -o $@                        \
	    -I ../include                \
	    -Wall -Wextra                \
	    -static                      \
	    -nostartfiles -nodefaultlibs \
	    -g -O0                       \
	    $^


clean:
	rm -f entry
