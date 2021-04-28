// Copyright (c) 2020 Johannes Stoelp

#include <io.h>

// API of `libgreet.so`.
extern const char* get_greet();
extern const char* get_greet2();
extern int gCalled;

void _start() {
    pfmt("Running _start() @ %s\n", __FILE__);

    // Call function from libgreet.so -> generates PLT relocations (R_X86_64_JUMP_SLOT).
    pfmt("get_greet()  -> %s\n", get_greet());
    pfmt("get_greet2() -> %s\n", get_greet2());

    // Reference global variable from libgreet.so -> generates RELA relocation (R_X86_64_COPY).
    pfmt("libgreet.so called %d times\n", gCalled);
}
