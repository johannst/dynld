// Copyright (c) 2020 Johannes Stoelp

#pragma once

#include <bits/stdint-uintn.h>
#include <stdint.h>

enum eAuxvTag {
    AT_NULL   =  0, /* ignored */
    AT_IGNORE =  1, /* ignored */
    AT_EXECFD =  2, /* val */
    AT_PHDR   =  3, /* ptr */
    AT_PHENT  =  4, /* val */
    AT_PHNUM  =  5, /* val */
    AT_PAGESZ =  6, /* val */
    AT_BASE   =  7, /* ptr */
    AT_FLAGS  =  8, /* val */
    AT_ENTRY  =  9, /* ptr */
    AT_NOTELF = 10, /* val */
    AT_UID    = 11, /* val */
    AT_EUID   = 12, /* val */
    AT_GID    = 13, /* val */
    AT_EGID   = 14, /* val */

    AT_MAX_CNT,
};
