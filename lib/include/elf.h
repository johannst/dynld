// Copyright (c) 2020 Johannes Stoelp

#pragma once

#include <stdint.h>

/// --------------
/// Program Header
/// --------------

#define PT_NULL    0 /* ignored */
#define PT_LOAD    1 /* Mark loadable segment (allowed p_memsz > p_filesz). */
#define PT_DYNAMIC 2 /* Location of .dynamic section */
#define PT_INTERP  3 /* Location of .interp section */
#define PT_NOTE    4 /* Location of auxiliary information */
#define PT_SHLIB   5 /* Reserved, but unspecified semantic */
#define PT_PHDR    6 /* Location & size of program headers itself */

#define PT_GNU_EH_FRAME 0x6474e550 /* [x86-64] stack unwinding tables */
#define PT_LOPROC       0x70000000
#define PT_HIPROC       0x7fffffff

#define PF_X 0x1 /* Phdr flag eXecute flag bitmask */
#define PF_W 0x2 /* Phdr flag Write flag bitmask */
#define PF_R 0x4 /* Phdr flag Read flag bitmask */

typedef struct {
    uint32_t type;    // Segment kind.
    uint32_t flags;   // Flags describing Segment attributes like R, W, X.
    uint64_t offset;  // Offset into the file where the Segment starts.
    uint64_t vaddr;   // Virtual address of first byte of Segment in memory.
    uint64_t paddr;   // Physical address, ignored in our case.
    uint64_t filesz;  // Number of bytes of the Segment in the file image.
    uint64_t memsz;   // Number of bytes of the segement in memory.
    uint64_t align;
} Elf64Phdr;

/// ---------------
/// Dynamic Section
/// ---------------

#define DT_NULL     0 /* [ignored] Marks end of dynamic section */
#define DT_NEEDED   1 /* [val] Name of needed library */
#define DT_PLTRELSZ 2 /* [val] Size in bytes of PLT relocs */
#define DT_PLTGOT   3 /* [ptr] Processor defined value */
#define DT_HASH     4 /* [ptr] Address of symbol hash table */
#define DT_STRTAB   5 /* [ptr] Address of string table */
#define DT_SYMTAB   6 /* [ptr] Address of symbol table */
#define DT_RELA     7 /* [ptr] Address of Rela relocs */
#define DT_RELASZ   8 /* [val] Total size of Rela relocs */
#define DT_RELAENT  9 /* [val] Size of one Rela reloc */
#define DT_STRSZ    10 /* [val] Size of string table */
#define DT_SYMENT   11 /* [val] Size of one symbol table entry */
#define DT_INIT     12 /* [ptr] Address of init function */
#define DT_FINI     13 /* [ptr] Address of termination function */
#define DT_SONAME   14 /* [val] Name of shared object */
#define DT_RPATH    15 /* [val] Library search path (deprecated) */
#define DT_SYMBOLIC 16 /* [ignored] Start symbol search here */
#define DT_REL      17 /* [ptr] Address of Rel relocs */
#define DT_RELSZ    18 /* [val] Total size of Rel relocs */
#define DT_RELENT   19 /* [val] Size of one Rel reloc */
#define DT_PLTREL   20 /* [val] Type of reloc in PLT */
#define DT_DEBUG    21 /* [ptr] For debugging; unspecified */
#define DT_TEXTREL  22 /* [ignored] Reloc might modify .text */
#define DT_JMPREL   23 /* [ptr] Address of PLT relocs */
#define DT_BIND_NOW 24 /* [ignored] Process relocations of object */
#define DT_MAX_CNT  25

typedef struct {
    uint64_t tag;
    union {
        uint64_t val;
        void* ptr;
    };
} Elf64Dyn;
