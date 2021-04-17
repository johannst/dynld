// Copyright (c) 2021 Johannes Stoelp

#include <common.h>

#if !defined(__linux__) || !defined(__x86_64__)
#    error "Only supported on linux(x86_64)!"
#endif

void* memset(void* s, int c, size_t n) {
    asm volatile(
        "cld"
        "\n"
        "rep stosb"
        : "+D"(s), "+c"(n)
        : "a"(c)
        : "memory");
    return s;
}
