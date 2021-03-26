// Copyright (c) 2021 Johannes Stoelp

#include <asm/unistd.h>  // __NR_*
#include <syscall.h>
#include <syscalls.h>

int open(const char* path, int flags) {
    return syscall2(__NR_open, path, flags);
}

ssize_t read(int fd, void* buf, size_t count) {
    return syscall3(__NR_read, fd, buf, count);
}

off_t lseek(int fd, off_t offset, int whence) {
    return syscall3(__NR_lseek, fd, offset, whence);
}

void _exit(int status) {
    syscall1(__NR_exit, status);
    __builtin_unreachable();
}
