# Copyright (c) 2020 Johannes Stoelp

COMMON_CFLAGS := -g -O0 -Wall -Wextra \
                 -I../lib/include     \
                 -nostartfiles -nodefaultlibs

run: main
	./$<

# Build the example user program.
#
# We explicitly set the dynamic linker to `dynld.so` and use the ELF hash table
# (DT_HASH), as we didn't implement support for the GNU hash table in our
# dynamic linker.
main: dynld.so libgreet.so main.c ../lib/libcommon.a
	gcc -o $@                                   \
	    $(COMMON_CFLAGS)                        \
	    -L$(CURDIR) -lgreet                     \
	    -Wl,--dynamic-linker=$(CURDIR)/dynld.so \
	    -Wl,--hash-style=sysv                   \
	    -no-pie \
	    $(filter %.c %.a, $^)

	#readelf -W --dynamic $@
	#readelf -W --program-headers $@
	#objdump --disassemble -j .plt -M intel $@
	#objdump --disassemble=_start -M intel $@

# Build the example shared library.
#
# We explicitly use the ELF hash table (DT_HASH), as we didn't implement
# support for the GNU hash table in our dynamic linker.
libgreet.so: libgreet.c
	gcc -o $@                 \
	    $(COMMON_CFLAGS)      \
	    -fPIC -shared         \
	    -Wl,--hash-style=sysv \
	    $^

# Build the dynamic linker.
#
# We assert that the dynamic linker doesn't contain any relocations as we
# didn't implement support to resolve its own relocations.
dynld.so: dynld.S dynld.c ../lib/libcommon.a
	gcc -o $@                          \
	    $(COMMON_CFLAGS)               \
	    -fPIC -static-pie              \
	    -fvisibility=hidden            \
	    -Wl,--entry=dl_start           \
	    -Wl,--no-allow-shlib-undefined \
	    $^

	@if ! readelf -r $@ | grep 'There are no relocations in this file' >& /dev/null; then \
		echo "ERROR: $@ contains relocations while we don't support relocations in $@!"; \
		exit 1; \
	fi

../lib/libcommon.a:
	make -C ../lib

clean:
	rm -f main libgreet.so
	rm -f dynld.so
