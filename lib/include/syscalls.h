// Copyright (c) 2021 Johannes Stoelp

#pragma once

#include <stddef.h>     // size_t
#include <sys/types.h>  // ssize_t, off_t, ...

// Syscall definitions taken from corresponding man pages, eg
//   open(2)
//   read(2)
//   ...

#define O_RDONLY 00
int open(const char* path, int flags);

ssize_t read(int fd, void* buf, size_t count);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
off_t lseek(int fd, off_t offset, int whence);

void _exit(int status);
