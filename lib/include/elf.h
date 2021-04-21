// Copyright (c) 2020 Johannes Stoelp

#pragma once

#include <stdint.h>

/// ----------
/// ELF Header
/// ----------

// Index into `ident`.
#define EI_MAG0  0
#define EI_MAG1  1
#define EI_MAG2  2
#define EI_MAG3  3
#define EI_CLASS 4
#define EI_DATA  5
#define EI_OSABI 7

// indent[EI_CLASS]
#define ELFCLASS32 1
#define ELFCLASS64 2

// indent[EI_CLASS]
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

// indent[EI_OSABI]
#define ELFOSABI_SYSV 0

// Objec file `type`.
#define ET_NONE 0
#define ET_DYN  3

typedef struct {
    uint8_t ident[16];   // ELF identification.
    uint16_t type;       // Object file type.
    uint16_t machine;    // Machine type.
    uint32_t version;    // Object file version.
    uint64_t entry;      // Entrypoint address.
    uint64_t phoff;      // Program header file offset.
    uint64_t shoff;      // Section header file offset.
    uint32_t flags;      // Processor specific flags.
    uint16_t ehsize;     // ELF header size.
    uint16_t phentsize;  // Program header entry size.
    uint16_t phnum;      // Number of program header entries.
    uint16_t shentsize;  // Section header entry size.
    uint16_t shnum;      // Number of section header entries.
    uint16_t shstrndx;   // Section name string table index.
} Elf64Ehdr;

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

#define DT_NULL     0  /* [ignored] Marks end of dynamic section */
#define DT_NEEDED   1  /* [val] Name of needed library */
#define DT_PLTRELSZ 2  /* [val] Size in bytes of PLT relocs */
#define DT_PLTGOT   3  /* [ptr] Processor defined value */
#define DT_HASH     4  /* [ptr] Address of symbol hash table */
#define DT_STRTAB   5  /* [ptr] Address of string table */
#define DT_SYMTAB   6  /* [ptr] Address of symbol table */
#define DT_RELA     7  /* [ptr] Address of Rela relocs */
#define DT_RELASZ   8  /* [val] Total size of Rela relocs */
#define DT_RELAENT  9  /* [val] Size of one Rela reloc */
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

/// ------------
/// Symbol Entry
/// ------------

typedef struct {
    uint32_t name;   // Symbol name (index into string table).
    uint8_t info;    // Symbol Binding bits[7..4] + Symbol Type bits[3..0].
    uint8_t other;   // Reserved.
    uint16_t shndx;  // Section table index.
    uint64_t value;  //
    uint64_t size;   //
} Elf64Sym;

#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i)&0xf)

// Symbold Bindings.
#define STB_GLOBAL 1 /* Global symbol, visible to all object files. */
#define STB_WEAK   2 /* Global scope, but with lower precedence than global symbols. */

// Symbol Types.
#define STT_NOTYPE 0 /* No type. */
#define STT_OBJECT 1 /* Data Object. */
#define STT_FUNC   2 /* Function entry point. */

// Special Section Indicies.
#define SHN_UNDEF 0     /* Undefined section. */
#define SHN_ABS   0xff1 /* Indicates an absolute value. */

/// -----------------
/// Relocations Entry
/// -----------------

typedef struct {
    uint64_t offset;  // Virtual address of the storage unit affected by the relocation.
    uint64_t info;    // Symbol table index + relocation type.
} Elf64Rel;

typedef struct {
    uint64_t offset;  // Virtual address of the storage unit affected by the relocation.
    uint64_t info;    // Symbol table index + relocation type.
    int64_t addend;   // Constant value used to compute the relocation value.
} Elf64Rela;

#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i)&0xffffffffL)

// x86_64 relocation types.
#define R_X86_64_COPY      5 /* Copy content from sym addr to relocation address */
#define R_X86_64_GLOB_DAT  6 /* Address affected by relocation: `offset` (+ base) */
#define R_X86_64_JUMP_SLOT 7 /* Address affected by relocation: `offset` (+ base) */
