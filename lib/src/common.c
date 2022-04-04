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
    // Cases to distinguish resulting from s and d pointers.
    //
    // Case 1 - same
    //   |------------|
    //   s            s+n
    //   d            d+n
    //
    //   -> Nothing to copy.
    //
    // Case 2 - disjunct
    //   |------------|       |------------|
    //   s            s+n     d            d+n
    //
    //   -> Nothing to worry, just copy the bytes from s to d.
    //
    // Case 3 - head overlap
    //         |------------|
    //         s            s+n
    //   |------------|
    //   d            d+n
    //
    //   -> Destructive copy for s but all bytes get properly copied from s to d.
    //      The user gets what he/she asked for.
    //
    // Case 4 - tail overlap
    //   |------------|
    //   s            s+n
    //         |------------|
    //         d            d+n
    //
    //   -> With a simple forward copy we would override the tail of s while
    //      copying into the head of d. This is destructive for s but we would
    //      also copy "wrong" bytes into d when we copy the tail of s (as it is
    //      already overwritten).
    //      This copy could be done properly by copying backwards (on x86 we
    //      could use the direction flag for string operations).
    //   -> We don't support this here as it is not needed any of the examples.

    // Case 4.
    ERROR_ON(s <= d && d < (void*)((unsigned char*)s + n), "memcpy: Unsupported overlap!");

    // Case 1.
    if (d == s) {
        return d;
    }

    // Case 2/3.
    asm volatile(
        "cld"
        "\n"
        "rep movsb"
        : "+D"(d), "+S"(s), "+c"(n)
        :
        : "memory");
    return d;
}
