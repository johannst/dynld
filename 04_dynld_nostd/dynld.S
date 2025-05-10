// SPDX-License-Identifier: MIT
//
// Copyright (c) 2020, Johannes Stoelp <dev@memzero.de>

#include <asm/unistd.h>

.intel_syntax noprefix

.section .text, "ax", @progbits
.global dl_start
dl_start:
    // $rsp is guaranteed to be 16-byte aligned.

    // Clear $rbp as specified by the SysV AMD64 ABI.
    xor rbp, rbp

    // Load pointer to process context prepared by execve(2) syscall as
    // specified in the SysV AMD64 ABI.
    // Save pointer in $rdi which is the arg0 (int/ptr) register.
    lea rdi, [rsp]

    // Stack frames must be 16-byte aligned before control is transfered to the
    // callees entry point.
    call dl_entry

    // Call exit(1) syscall to indicate error, dl_entry should not return.
    mov rdi, 1
    mov rax, __NR_exit
    syscall
