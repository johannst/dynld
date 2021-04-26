// Copyright (c) 2020 Johannes Stoelp

#include <io.h>

int gCalled = 0;

const char* get_greet() {
    // Reference global variable -> generates RELA relocation (R_X86_64_GLOB_DAT).
    ++gCalled;
    return "Hello from libgreet.so!";
}

const char* get_greet2() {
    // Reference global variable -> generates RELA relocation (R_X86_64_GLOB_DAT).
    ++gCalled;
    return "Hello 2 from libgreet.so!";
}

// Definition of `static` function which is referenced from the `INIT` dynamic
// section entry -> generates R_X86_64_RELATIVE relocation.
__attribute__((constructor)) static void libinit() {
    pfmt("libgreet.so: libinit\n");
}

// Definition of `non static` function which is referenced from the `FINI`
// dynamic section entry -> generates R_X86_64_64 relocation.
__attribute__((destructor)) void libfini() {
    pfmt("libgreet.so: libfini\n");
}
