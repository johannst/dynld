# Copyright (c) 2020 Johannes Stoelp

HDR+=include/alloc.h
HDR+=include/auxv.h
HDR+=include/elf.h
HDR+=include/fmt.h
HDR+=include/io.h
HDR+=include/syscall.h
HDR+=include/syscalls.h

DEP+=src/alloc.o
DEP+=src/common.o
DEP+=src/fmt.o
DEP+=src/io.o
DEP+=src/syscalls.o

libcommon.a: $(HDR) $(DEP)
	ar -crs $@ $(filter %.o, $^)

src/%.o: src/%.c
	gcc -c -o $@            \
	    -g -O0              \
	    -Wall -Wextra       \
	    -I$(CURDIR)/include \
	    -nostdlib           \
	    $<

clean:
	rm -f $(DEP)
	rm -f libcommon.a
