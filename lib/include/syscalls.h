// Copyright (c) 2021 Johannes Stoelp

#pragma once

#include <stddef.h>     // size_t
#include <sys/types.h>  // ssize_t, off_t, ...

extern int dynld_errno;

// Syscall definitions taken from corresponding man pages, eg
//   open(2)
//   read(2)
//   ...

#define O_RDONLY 00
int open(const char* path, int flags);
int close(int fd);

#define F_OK 0
#define R_OK 4
int access(const char* path, int mode);

ssize_t write(int fd, const void* buf, size_t count);
ssize_t read(int fd, void* buf, size_t count);
ssize_t pread(int fd, void* buf, size_t count, off_t offset);

// mmap - prot:
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4
// mmap - flags:
#define MAP_PRIVATE   0x2
#define MAP_ANONYMOUS 0x20
#define MAP_FIXED     0x10
// mmap - ret:
#define MAP_FAILED ((void*)-1)
void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void* addr, size_t length);

void _exit(int status);
