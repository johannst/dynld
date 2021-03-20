// Copyright (c) 2021 Johannes Stoelp

#include <syscall.h>
#include <io.h>

#include <asm/unistd.h>

void _start() {
    pfmt("Running %s @ %s\n", __FUNCTION__, __FILE__);
    syscall1(__NR_exit, 0);
}
