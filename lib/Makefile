# Copyright (c) 2020 Johannes Stoelp

HDR+=include/auxv.h
HDR+=include/elf.h
HDR+=include/fmt.h
HDR+=include/io.h
HDR+=include/syscall.h

DEP+=src/io.o
DEP+=src/fmt.o

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
