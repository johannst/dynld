// Copyright (c) 2020 Johannes Stoelp

#pragma once

#include "io.h"
#include "syscalls.h"

#define ERROR_ON(cond, fmt, ...)                                               \
    do {                                                                       \
        if ((cond)) {                                                          \
            efmt("%s:%d " fmt, __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__); \
            _exit(1);                                                          \
        }                                                                      \
    } while (0)
