// SPDX-License-Identifier: MIT
//
// Copyright (c) 2020, Johannes Stoelp <dev@memzero.de>

#pragma once

#include <stdarg.h>

int vfmt(char* buf, unsigned long len, const char* fmt, va_list ap);
int fmt(char* buf, unsigned long len, const char* fmt, ...);
