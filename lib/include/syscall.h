// Copyright (c) 2020 Johannes Stoelp

#pragma once

#if !defined(__linux__) || !defined(__x86_64__)
#    error "Only supported on linux(x86_64)!"
#endif

// Inline ASM
//   Syntax:
//     asm asm-qualifiers (AssemblerTemplate : OutputOperands : InputOperands : Clobbers)
//
//   Output operand constraints:
//     = | operand (variable) is written to by this instruction
//     + | operand (variable) is written to / read from by this instruction
//
//   Input/Output operand constraints:
//     r | allocate general purpose register
//
//   Machine specific constraints (x86_64):
//     a  | a register (eg rax)
//     d  | d register (eg rdx)
//     D  | di register (eg rdi)
//     S  | si register (eg rsi)
//
//  Local register variables:
//    In case a specific register is required which can not be specified via a
//    machine specific constraint.
//    ```c
//    register long r12 asm ("r12") = 42;
//    asm("nop" : : "r"(r12));
//    ```
//
// Reference:
//   https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
//   https://gcc.gnu.org/onlinedocs/gcc/Machine-Constraints.html#Machine-Constraints
//   https://gcc.gnu.org/onlinedocs/gcc/Local-Register-Variables.html

// Linux syscall ABI
//  x86-64
//    #syscall: rax
//    ret     : rax
//    instr   : syscall
//    args    : rdi   rsi   rdx   r10   r8    r9
//
// Reference:
//   syscall(2)

#define argcast(A)              ((long)(A))
#define syscall1(n, a1)         _syscall1(n, argcast(a1))
#define syscall3(n, a1, a2, a3) _syscall3(n, argcast(a1), argcast(a2), argcast(a3))

static inline long _syscall1(long n, long a1) {
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "memory");
    return ret;
}

static inline long _syscall3(long n, long a1, long a2, long a3) {
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "memory");
    return ret;
}
