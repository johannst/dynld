// Copyright (c) 2020 Johannes Stoelp

#include <io.h>
#include <fmt.h>
#include <syscall.h>

#include <asm/unistd.h>

// `pfmt` uses fixed-size buffer on the stack for formating the message
// (for simplicity and since we don't impl buffered I/O).
//
// NOTE: This allows to specify a large buffer on the stack, but for
// the purpose of this study that's fine, we are cautious.
#define MAX_PRINTF_LEN 128

#define FD_STDOUT 1
#define FD_STDERR 2

int pfmt(const char* fmt, ...) {
    char buf[MAX_PRINTF_LEN];

    va_list ap;
    va_start(ap, fmt);
    int ret = vfmt(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (ret > MAX_PRINTF_LEN - 1) {
        syscall3(__NR_write, FD_STDERR, buf, MAX_PRINTF_LEN - 1);

        static const char warn[] = "\npfmt: Message truncated, max length can be configured by defining MAX_PRINTF_LEN\n";
        syscall3(__NR_write, FD_STDOUT, warn, sizeof(warn));
        return MAX_PRINTF_LEN - 1;
    }

    syscall3(__NR_write, FD_STDOUT, buf, ret);
    return ret;
}