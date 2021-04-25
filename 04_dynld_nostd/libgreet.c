// Copyright (c) 2020 Johannes Stoelp

#include <io.h>

int gCalled = 0;

const char* get_greet() {
    ++gCalled;
    return "Hello from libgreet.so!";
}

const char* get_greet2() {
    ++gCalled;
    return "Hello 2 from libgreet.so!";
}

__attribute__((constructor)) static void libinit() { /* static -> generates R_X86_64_RELATIVE relocation */
    pfmt("libgreet.so: libinit\n");
}

__attribute__((destructor)) void libfini() { /* non static -> generates R_X86_64_64 relocation */
    pfmt("libgreet.so: libfini\n");
}
