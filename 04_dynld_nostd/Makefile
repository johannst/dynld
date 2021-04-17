# Copyright (c) 2020 Johannes Stoelp

COMMON_CFLAGS := -g -O0 -Wall -Wextra \
                 -I../lib/include     \
                 -nostartfiles -nodefaultlibs

run: main
	./$<

main: dynld.so main.c ../lib/libcommon.a
	@# For now ew only add support for ELF hash tables (DT_HASH).
	@# Therefore we specify the `hash-style` below.
	gcc -o libgreet.so        \
	    $(COMMON_CFLAGS)      \
	    -fPIC -shared         \
	    -Wl,--hash-style=sysv \
	    libgreet.c

	@if readelf -W -S libgreet.so | grep plt >& /dev/null; then \
		echo "ERROR: libgreet.so contains PLT while we don't support relocation calls in libgreet.so!"; \
		echo "       All function calls in libgreet.so must be statically resolved!"; \
		exit 1; \
	fi

	@# For now ew only add support for ELF hash tables (DT_HASH).
	@# Therefore we specify the `hash-style` below.
	gcc -o $@                                   \
	    $(COMMON_CFLAGS)                        \
	    -L$(CURDIR) -lgreet                     \
	    -Wl,--dynamic-linker=$(CURDIR)/dynld.so \
	    -Wl,--hash-style=sysv                   \
	    -no-pie \
	    $(filter %.c %.a, $^)

	readelf -W --dynamic $@
	readelf -W --program-headers $@
	objdump --disassemble -j .plt -M intel $@
	objdump --disassemble=_start -M intel $@

dynld.so: dynld.S dynld.c ../lib/libcommon.a
	gcc -o $@                          \
	    $(COMMON_CFLAGS)               \
	    -fPIC -static-pie              \
	    -fvisibility=hidden            \
	    -Wl,--entry=dl_start           \
	    -Wl,--no-allow-shlib-undefined \
	    $^

	@if readelf -W -S $@ | grep plt >& /dev/null; then \
		echo "ERROR: $@ contains PLT while we don't support relocation calls in $@!"; \
		echo "       All function calls in $@ must be statically resolved!"; \
		exit 1; \
	fi

../lib/libcommon.a:
	make -C ../lib

clean:
	rm -f main libgreet.so
	rm -f dynld.so
