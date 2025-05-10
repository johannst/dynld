// SPDX-License-Identifier: MIT
//
// Copyright (c) 2020, Johannes Stoelp <dev@memzero.de>

#pragma once

#include "io.h"
#include "syscalls.h"

#define ERROR_ON(cond, fmt, ...)                                        \
    do {                                                                \
        if ((cond)) {                                                   \
            efmt("%s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
            _exit(1);                                                   \
        }                                                               \
    } while (0)


void* memset(void* s, int c, size_t n);
void* memcpy(void* d, const void* s, size_t n);

