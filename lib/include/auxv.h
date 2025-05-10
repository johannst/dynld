// SPDX-License-Identifier: MIT
//
// Copyright (c) 2020, Johannes Stoelp <dev@memzero.de>

#pragma once

#include <stdint.h>

/// ----------------
/// Auxiliary Vector
/// ----------------

// NOTE: [x86-64] Either AT_EXECFD or AT_PHDR must be supplied by the Kernel.

#define AT_NULL    0  /* [ignored] Mark end of auxiliary vetcor */
#define AT_IGNORE  1  /* [ignored] */
#define AT_EXECFD  2  /* [val] File descriptor of user program (in case Linux Kernel didn't mapped) */
#define AT_PHDR    3  /* [ptr] Address of Phdr of use program (in case Kernel mapped user program) */
#define AT_PHENT   4  /* [val] Size in bytes of one Phdr entry */
#define AT_PHNUM   5  /* [val] Number of Phdr entries */
#define AT_PAGESZ  6  /* [val] System page size */
#define AT_BASE    7  /* [ptr] `base address` interpreter was loaded to */
#define AT_FLAGS   8  /* [val] */
#define AT_ENTRY   9  /* [ptr] Entry point of user program */
#define AT_NOTELF  10 /* [val] >0 if not an ELF file */
#define AT_UID     11 /* [val] Real user id of process */
#define AT_EUID    12 /* [val] Effective user id of process */
#define AT_GID     13 /* [val] Real group id of process */
#define AT_EGID    14 /* [val] Effective user id of process */
#define AT_MAX_CNT 15

typedef struct {
    uint64_t tag;
    union {
        uint64_t val;
        void* ptr;
    };
} Auxv64Entry;
