// Copyright (c) 2020 Johannes Stoelp

#include <fmt.h>
#include <io.h>
#include <syscall.h>
#include <syscalls.h>

// `pfmt` uses fixed-size buffer on the stack for formating the message
// (for simplicity and since we don't impl buffered I/O).
//
// NOTE: This allows to specify a large buffer on the stack, but for
// the purpose of this study that's fine, we are cautious.
#define MAX_PRINTF_LEN 128

#define FD_STDOUT 1
#define FD_STDERR 2

static int vdfmt(int fd, const char* fmt, va_list ap) {
    char buf[MAX_PRINTF_LEN];
    int ret = vfmt(buf, sizeof(buf), fmt, ap);

    if (ret > MAX_PRINTF_LEN - 1) {
        write(fd, buf, MAX_PRINTF_LEN - 1);

        static const char warn[] = "\npfmt: Message truncated, max length can be configured by defining MAX_PRINTF_LEN\n";
        write(FD_STDERR, warn, sizeof(warn));
        return MAX_PRINTF_LEN - 1;
    }

    write(fd, buf, ret);
    return ret;
}

int pfmt(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vdfmt(FD_STDOUT, fmt, ap);
    va_end(ap);
    return ret;
}

int efmt(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vdfmt(FD_STDERR, fmt, ap);
    va_end(ap);
    return ret;
}
