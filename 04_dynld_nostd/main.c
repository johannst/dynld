// Copyright (c) 2020 Johannes Stoelp

#include <io.h>
#include <syscalls.h>

// API of `libgreet.so`.
extern const char* get_greet();
extern const char* get_greet2();

void _start() {
    pfmt("Running _start() @ %s\n", __FILE__);

    // Call function from libgreet.so -> generates PLT entry.
    pfmt("get_greet()  -> %s\n", get_greet());
    pfmt("get_greet2() -> %s\n", get_greet2());

    _exit(0);
}
