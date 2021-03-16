// Copyright (c) 2020 Johannes Stoelp

#include <asm/unistd.h>

#if !defined(__linux__) || !defined(__x86_64__)
#    error "Only supported in linux(x86_64)!"
#endif

.intel_syntax noprefix

.section .text, "ax", @progbits
.global _start
_start:
    // $rsp is guaranteed to be 16-byte aligned.

    // Clear $rbp as specified by the SysV AMD64 ABI.
    xor rbp, rbp

    // Load pointer to process context prepared by execve(2) syscall as
    // specified in the SysV AMD64 ABI.
    // Save pointer in $rdi which is the arg0 (int/ptr) register.
    lea rdi, [rsp]

    // Stack frames must be 16-byte aligned before control is transfered to the
    // callees entry point.
    call entry

    // Call exit(0) syscall.
    mov rdi, 0
    mov rax, __NR_exit
    syscall
