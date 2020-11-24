// Copyright (c) 2020 Johannes Stoelp

#pragma once

#include "fmt.h"
#include "syscall.h"

#include <asm/unistd.h>

// `dynld_printf` uses fixed-size buffer on the stack for formating the message
// (since we don't impl buffered I/O).
//
// Size can be re-configured by defining `MAX_PRINTF_LEN` before including
// `io.h`.
//
// NOTE: This allows to specify an arbitrarily large buffer on the stack, but
// for the purpose of this study that's fine, we are cautious.
#if !defined(MAX_PRINTF_LEN)
#    define MAX_PRINTF_LEN 64
#endif

#define FD_STDOUT 1
#define FD_STDERR 2

int dynld_printf(const char* fmt, ...) {
    char buf[MAX_PRINTF_LEN];

    va_list ap;
    va_start(ap, fmt);
    int ret = dynld_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (ret > MAX_PRINTF_LEN - 1) {
        syscall3(__NR_write, FD_STDERR, buf, MAX_PRINTF_LEN - 1);

        static const char warn[] = "\ndynld_printf: Message truncated, max length can be configured by defining MAX_PRINTF_LEN\n";
        syscall3(__NR_write, FD_STDOUT, warn, sizeof(warn));
        return MAX_PRINTF_LEN - 1;
    }

    syscall3(__NR_write, FD_STDOUT, buf, ret);
    return ret;
}
