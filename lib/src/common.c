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

void* memcpy(void* d, const void* s, size_t n) {
    // When `d` points into `[s, s+n[` we would override `s` while copying into `d`.
    //   |------------|--------|
    //   s            d        s+n
    // -> We don't support.
    //
    // When `d` points into `]s-n, s[` it is destructive for `s` but all data
    // from `s` are copied into `d`. The user gets what he asks for.
    // -> Supported.
    ERROR_ON(s <= d && d < (void*)((unsigned char*)s + n), "memcpy: Unsupported overlap!");
    asm volatile(
        "cld"
        "\n"
        "rep movsb"
        : "+D"(d), "+S"(s), "+c"(n)
        :
        : "memory");
    return d;
}
