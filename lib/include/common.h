// Copyright (c) 2020 Johannes Stoelp

#pragma once

#include "io.h"
#include "syscall.h"

#include <asm/unistd.h>

#define ERROR_ON(cond, ...)         \
    do {                            \
        if ((cond)) {               \
            efmt(__VA_ARGS__);      \
            syscall1(__NR_exit, 1); \
        }                           \
    } while (0)
