// Copyright (c) 2021 Johannes Stoelp

#include <asm/unistd.h>  // __NR_*
#include <syscall.h>
#include <syscalls.h>

// Storage for `dynld_errno`.
int dynld_errno;

// Convert return value to errno/ret.
static long syscall_ret(unsigned long ret) {
    if (ret > (unsigned long)-4096ul) {
        dynld_errno = -ret;
        return -1;
    }
    return ret;
}

int open(const char* path, int flags) {
    long ret = syscall2(__NR_open, path, flags);
    return syscall_ret(ret);
}

int close(int fd) {
    long ret = syscall1(__NR_close, fd);
    return syscall_ret(ret);
}

int access(const char* path, int mode) {
    long ret = syscall2(__NR_access, path, mode);
    return syscall_ret(ret);
}

ssize_t write(int fd, const void* buf, size_t count) {
    long ret = syscall3(__NR_write, fd, buf, count);
    return syscall_ret(ret);
}

ssize_t read(int fd, void* buf, size_t count) {
    long ret = syscall3(__NR_read, fd, buf, count);
    return syscall_ret(ret);
}

ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
    long ret = syscall4(__NR_read, fd, buf, count, offset);
    return syscall_ret(ret);
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    long ret = syscall6(__NR_mmap, addr, length, prot, flags, fd, offset);
    return (void*)syscall_ret(ret);
}

int munmap(void* addr, size_t length) {
    long ret = syscall2(__NR_munmap, addr, length);
    return syscall_ret(ret);
}

void _exit(int status) {
    syscall1(__NR_exit, status);
    __builtin_unreachable();
}
