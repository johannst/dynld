// SPDX-License-Identifier: MIT
//
// Copyright (c) 2021, Johannes Stoelp <dev@memzero.de>

#include <auxv.h>
#include <common.h>
#include <elf.h>
#include <io.h>
#include <syscalls.h>

#include <stdbool.h>
#include <stdint.h>

// {{{ Global constans

enum {
    // Hard-coded page size.
    // We assert against the `AT_PAGESZ` auxiliary vector entry.
    PAGE_SIZE = 4096,
    // Hard-coded upper limit of `DT_NEEDED` entries per dso
    // (for simplicity to not require allocations).
    MAX_NEEDED = 1,
};

// }}}
// {{{ SystemVDescriptor

typedef struct {
    uint64_t argc;              // Number of commandline arguments.
    const char** argv;          // List of pointer to command line arguments.
    uint64_t envc;              // Number of environment variables.
    const char** envv;          // List of pointers to environment variables.
    uint64_t auxv[AT_MAX_CNT];  // Auxiliary vector entries.
} SystemVDescriptor;

// Interpret and extract data passed on the stack by the Linux Kernel
// when loading the initial process image.
// The data is organized according to the SystemV x86_64 ABI.
static SystemVDescriptor get_systemv_descriptor(const uint64_t* prctx) {
    SystemVDescriptor sysv = {0};

    sysv.argc = *prctx;
    sysv.argv = (const char**)(prctx + 1);
    sysv.envv = (const char**)(sysv.argv + sysv.argc + 1);

    // Count the number of environment variables in the `ENVP` segment.
    for (const char** env = sysv.envv; *env; ++env) {
        sysv.envc += 1;
    }

    // Decode auxiliary vector `AUXV`.
    for (const Auxv64Entry* auxvp = (const Auxv64Entry*)(sysv.envv + sysv.envc + 1); auxvp->tag != AT_NULL; ++auxvp) {
        if (auxvp->tag < AT_MAX_CNT) {
            sysv.auxv[auxvp->tag] = auxvp->val;
        }
    }

    return sysv;
}

// }}}
// {{{ Dso

typedef struct {
    uint8_t* base;                 // Base address.
    void (*entry)();               // Entry function.
    uint64_t dynamic[DT_MAX_CNT];  // `.dynamic` section entries.
    uint64_t needed[MAX_NEEDED];   // Shared object dependencies (`DT_NEEDED` entries).
    uint32_t needed_len;           // Number of `DT_NEEDED` entries (SO dependencies).
} Dso;

static void decode_dynamic(Dso* dso, uint64_t dynoff) {
    // Decode `.dynamic` section of the `dso`.
    for (const Elf64Dyn* dyn = (const Elf64Dyn*)(dso->base + dynoff); dyn->tag != DT_NULL; ++dyn) {
        if (dyn->tag == DT_NEEDED) {
            ERROR_ON(dso->needed_len == MAX_NEEDED, "Too many dso dependencies!");
            dso->needed[dso->needed_len++] = dyn->val;
        } else if (dyn->tag < DT_MAX_CNT) {
            dso->dynamic[dyn->tag] = dyn->val;
        }
    }

    // Check for string table entries.
    ERROR_ON(dso->dynamic[DT_STRTAB] == 0, "DT_STRTAB missing in dynamic section!");
    ERROR_ON(dso->dynamic[DT_STRSZ] == 0, "DT_STRSZ missing in dynamic section!");

    // Check for symbol table entries.
    ERROR_ON(dso->dynamic[DT_SYMTAB] == 0, "DT_SYMTAB missing in dynamic section!");
    ERROR_ON(dso->dynamic[DT_SYMENT] == 0, "DT_SYMENT missing in dynamic section!");
    ERROR_ON(dso->dynamic[DT_SYMENT] != sizeof(Elf64Sym), "ELf64Sym size miss-match!");

    // Check for SystemV hash table. We only support SystemV hash tables
    // `DT_HASH`, not gnu hash tables `DT_GNU_HASH`.
    ERROR_ON(dso->dynamic[DT_HASH] == 0, "DT_HASH missing in dynamic section!");
}

static Dso get_prog_dso(const SystemVDescriptor* sysv) {
    Dso prog = {0};

    // Determine the base address of the user program.
    //   We only support the case where the Kernel already mapped the
    // user program into the virtual address space and therefore the
    // auxiliary vector contains an `AT_PHDR` entry pointing to the
    // Program Headers of the user program.
    // In that case, the base address of the user program can be
    // computed by taking the absolute address of the `AT_PHDR` entry
    // and subtracting the relative address `p_vaddr` of the `PT_PHDR`
    // entry from the user programs Program Header iself.
    //
    //              VMA
    //              |         |
    // PROG BASE -> |         |  ^
    //              |         |  |
    //              |         |  | <---------------------+
    //              |         |  |                       |
    //   AT_PHDR -> +---------+  v                       |
    //              |         |                          |
    //              |         |                          |
    //              | PT_PHDR | -----> Elf64Phdr { .., vaddr, .. }
    //              |         |
    //              |         |
    //              +---------+
    //              |         |
    //
    // PROG BASE = AT_PHDR - PT_PHDR.vaddr
    ERROR_ON(sysv->auxv[AT_PHDR] == 0 || sysv->auxv[AT_EXECFD] != 0, "AT_PHDR entry missing in the AUXV!");

    // Offset to the `.dynamic` section from the user programs `base addr`.
    uint64_t dynoff = 0;

    // Program header of the user program.
    const Elf64Phdr* phdr = (const Elf64Phdr*)sysv->auxv[AT_PHDR];

    ERROR_ON(sysv->auxv[AT_PHENT] != sizeof(Elf64Phdr), "Elf64Phdr size miss-match!");

    // Decode PHDRs of the user program.
    for (unsigned phdrnum = sysv->auxv[AT_PHNUM]; --phdrnum; ++phdr) {
        if (phdr->type == PT_PHDR) {
            ERROR_ON(sysv->auxv[AT_PHDR] < phdr->vaddr, "Expectation auxv[AT_PHDR] >= phdr->vaddr failed!");
            prog.base = (uint8_t*)(sysv->auxv[AT_PHDR] - phdr->vaddr);
        } else if (phdr->type == PT_DYNAMIC) {
            dynoff = phdr->vaddr;
        }

        ERROR_ON(phdr->type == PT_TLS, "Thread local storage not supported found PT_TLS!");
    }
    ERROR_ON(dynoff == 0, "PT_DYNAMIC entry missing in the user programs PHDR!");

    // Decode `.dynamic` section.
    decode_dynamic(&prog, dynoff);

    // Get the entrypoint of the user program form the auxiliary vector.
    ERROR_ON(sysv->auxv[AT_ENTRY] == 0, "AT_ENTRY entry missing in the AUXV!");
    prog.entry = (void (*)())sysv->auxv[AT_ENTRY];

    return prog;
}

static uint64_t get_num_dynsyms(const Dso* dso) {
    ERROR_ON(dso->dynamic[DT_HASH] == 0, "DT_HASH missing in dynamic section!");

    // Get SystemV hash table.
    const uint32_t* hashtab = (const uint32_t*)(dso->base + dso->dynamic[DT_HASH]);

    // SystemV hash table layout:
    //   nbucket
    //   nchain
    //   bucket[nbuckets]
    //   chain[nchains]
    //
    // From the SystemV ABI - Dynamic Linking - Hash Table:
    //   Both `bucket` and `chain` hold symbol table indexes. Chain
    //   table entries parallel the symbol table. The number of symbol
    //   table entries should equal `nchain`.
    return hashtab[1];
}

static const char* get_str(const Dso* dso, uint64_t idx) {
    ERROR_ON(dso->dynamic[DT_STRSZ] < idx, "String table indexed out-of-bounds!");
    return (const char*)(dso->base + dso->dynamic[DT_STRTAB] + idx);
}

static const Elf64Sym* get_sym(const Dso* dso, uint64_t idx) {
    ERROR_ON(get_num_dynsyms(dso) < idx, "Symbol table index out-of-bounds!");
    return (const Elf64Sym*)(dso->base + dso->dynamic[DT_SYMTAB]) + idx;
}

static const Elf64Rela* get_pltreloca(const Dso* dso, uint64_t idx) {
    ERROR_ON(dso->dynamic[DT_PLTRELSZ] < sizeof(Elf64Rela) * idx, "PLT relocation table indexed out-of-bounds!");
    return (const Elf64Rela*)(dso->base + dso->dynamic[DT_JMPREL]) + idx;
}

static const Elf64Rela* get_reloca(const Dso* dso, uint64_t idx) {
    ERROR_ON(dso->dynamic[DT_RELASZ] < sizeof(Elf64Rela) * idx, "RELA relocation table indexed out-of-bounds!");
    return (const Elf64Rela*)(dso->base + dso->dynamic[DT_RELA]) + idx;
}

// }}}
// {{{ Init & Fini

typedef void (*initfptr)();

static void init(const Dso* dso) {
    if (dso->dynamic[DT_INIT]) {
        initfptr* fn = (initfptr*)(dso->base + dso->dynamic[DT_INIT]);
        (*fn)();
    }

    size_t nfns = dso->dynamic[DT_INIT_ARRAYSZ] / sizeof(initfptr);
    initfptr* fns = (initfptr*)(dso->base + dso->dynamic[DT_INIT_ARRAY]);
    while (nfns--) {
        (*fns++)();
    }
}

typedef void (*finifptr)();

static void fini(const Dso* dso) {
    size_t nfns = dso->dynamic[DT_FINI_ARRAYSZ] / sizeof(finifptr);
    finifptr* fns = (finifptr*)(dso->base + dso->dynamic[DT_FINI_ARRAY]) + nfns /* reverse destruction order */;
    while (nfns--) {
        (*--fns)();
    }

    if (dso->dynamic[DT_FINI]) {
        finifptr* fn = (finifptr*)(dso->base + dso->dynamic[DT_FINI]);
        (*fn)();
    }
}

// }}}
// {{{ Symbol lookup

static inline int strcmp(const char* s1, const char* s2) {
    while (*s1 == *s2 && *s1) {
        ++s1;
        ++s2;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// Perform naive lookup for global symbol and return address if symbol was found.
//
// For simplicity this lookup doesn't use the hash table (`DT_HASH` |
// `DT_GNU_HASH`) but rather iterates of the dynamic symbol table. Using the
// hash table doesn't change the lookup result, however it yields better
// performance for large symbol tables.
//
// `dso`          A handle to the dso which dynamic symbol table should be searched.
// `symname`     Name of the symbol to look up.
static void* lookup_sym(const Dso* dso, const char* symname) {
    for (unsigned i = 0; i < get_num_dynsyms(dso); ++i) {
        const Elf64Sym* sym = get_sym(dso, i);

        if ((ELF64_ST_TYPE(sym->info) == STT_OBJECT || ELF64_ST_TYPE(sym->info) == STT_FUNC) && ELF64_ST_BIND(sym->info) == STB_GLOBAL &&
            sym->shndx != SHN_UNDEF) {
            if (strcmp(symname, get_str(dso, sym->name)) == 0) {
                return dso->base + sym->value;
            }
        }
    }
    return 0;
}

// }}}
// {{{ Map Shared Library Dependency

static Dso map_dependency(const char* dependency) {
    // For simplicity we only search for SO dependencies in the current working dir.
    // So no support for DT_RPATH/DT_RUNPATH and LD_LIBRARY_PATH.
    ERROR_ON(access(dependency, R_OK) != 0, "Dependency '%s' does not exist!\n", dependency);

    const int fd = open(dependency, O_RDONLY);
    ERROR_ON(fd < 0, "Failed to open '%s'", dependency);

    Elf64Ehdr ehdr;
    // Read ELF header.
    ERROR_ON(read(fd, &ehdr, sizeof(ehdr)) != (ssize_t)sizeof(ehdr), "Failed to read Elf64Ehdr!");

    // Check ELF magic.
    ERROR_ON(ehdr.ident[EI_MAG0] != '\x7f' || ehdr.ident[EI_MAG1] != 'E' || ehdr.ident[EI_MAG2] != 'L' || ehdr.ident[EI_MAG3] != 'F',
             "Dependency '%s' wrong ELF magic value!\n", dependency);
    // Check ELF header size.
    ERROR_ON(ehdr.ehsize != sizeof(ehdr), "Elf64Ehdr size miss-match!");
    // Check for 64bit ELF.
    ERROR_ON(ehdr.ident[EI_CLASS] != ELFCLASS64, "Dependency '%s' is not 64bit ELF!\n", dependency);
    // Check for OS ABI.
    ERROR_ON(ehdr.ident[EI_OSABI] != ELFOSABI_SYSV, "Dependency '%s' is not built for SysV OS ABI!\n", dependency);
    // Check ELF type.
    ERROR_ON(ehdr.type != ET_DYN, "Dependency '%s' is not a dynamic library!");
    // Check for Phdr.
    ERROR_ON(ehdr.phnum == 0, "Dependency '%s' has no Phdr!\n", dependency);


    Elf64Phdr phdr[ehdr.phnum];
    // Check PHDR header size.
    ERROR_ON(ehdr.phentsize != sizeof(phdr[0]), "Elf64Phdr size miss-match!");

    // Read Program headers at offset `phoff`.
    ERROR_ON(pread(fd, &phdr, sizeof(phdr), ehdr.phoff) != (ssize_t)sizeof(phdr), "Failed to read Elf64Phdr[%d]!\n", ehdr.phnum);

    // Compute start and end address used by the library based on the all the `PT_LOAD` program headers.
    uint64_t dynoff = 0;
    uint64_t addr_start = (uint64_t)-1;
    uint64_t addr_end = 0;
    for (unsigned i = 0; i < ehdr.phnum; ++i) {
        const Elf64Phdr* p = &phdr[i];
        if (p->type == PT_DYNAMIC) {
            // Offset to `.dynamic` section.
            dynoff = p->vaddr;
        } else if (p->type == PT_LOAD) {
            // Find start & end address.
            if (p->vaddr < addr_start) {
                addr_start = p->vaddr;
            } else if (p->vaddr + p->memsz > addr_end) {
                addr_end = p->vaddr + p->memsz;
            }
        }

        ERROR_ON(phdr->type == PT_TLS, "Thread local storage not supported found PT_TLS!");
    }

    // Align start address to the next lower page boundary.
    addr_start = addr_start & ~(PAGE_SIZE - 1);
    // Align end address to the next higher page boundary.
    addr_end = (addr_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Reserve region big enough to map all `PT_LOAD` sections of `dependency`.
    uint8_t* map = mmap(0 /* addr */, addr_end - addr_start /* len */, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS,
                        -1 /* fd */, 0 /* file offset */);
    ERROR_ON(map == MAP_FAILED, "Failed to mmap address space for dependency '%s'\n", dependency);

    // Compute base address for library.
    uint8_t* base = map - addr_start;

    // Map in all `PT_LOAD` segments from the `dependency`.
    for (unsigned i = 0; i < ehdr.phnum; ++i) {
        const Elf64Phdr* p = &phdr[i];
        if (p->type != PT_LOAD) {
            continue;
        }

        // Page align start & end address.
        uint64_t addr_start = p->vaddr & ~(PAGE_SIZE - 1);
        uint64_t addr_end = (p->vaddr + p->memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        // Page align file offset.
        uint64_t off = p->offset & ~(PAGE_SIZE - 1);

        // Compute segment permissions.
        uint32_t prot = (p->flags & PF_X ? PROT_EXEC : 0) | (p->flags & PF_R ? PROT_READ : 0) | (p->flags & PF_W ? PROT_WRITE : 0);

        // Mmap segment.
        ERROR_ON(mmap(base + addr_start, addr_end - addr_start, prot, MAP_PRIVATE | MAP_FIXED, fd, off) != base + addr_start,
                 "Failed to map `PT_LOAD` section %d for dependency '%s'.", i, dependency);

        // From the SystemV ABI - Program Headers:
        //   If the segment’s memorysize (memsz) is larger than the file size (filesz), the "extra" bytes are defined to hold the value
        //   `0` and to follow the segment’s initialized are
        //
        // This is typically used by the `.bss` section.
        if (p->memsz > p->filesz) {
            memset(base + p->vaddr + p->filesz, 0 /* byte */, p->memsz - p->filesz /*len*/);
        }
    }

    // Close file descriptor.
    close(fd);

    Dso dso = {0};
    dso.base = base;
    decode_dynamic(&dso, dynoff);
    return dso;
}

// }}}
// {{{ Resolve relocations

typedef struct LinkMap {
    const Dso* dso;              // Pointer to Dso list object.
    const struct LinkMap* next;  // Pointer to next LinkMap entry ('0' terminates the list).
} LinkMap;

// Resolve a single relocation of `dso`.
//
// Resolve the relocation `reloc` by looking up the address of the symbol
// referenced by the relocation. If the address of the symbol was found the
// relocation is patched, if the address was not found the process exits.
static void resolve_reloc(const Dso* dso, const LinkMap* map, const Elf64Rela* reloc) {
    // Get symbol referenced by relocation.
    const int symidx = ELF64_R_SYM(reloc->info);
    const Elf64Sym* sym = get_sym(dso, symidx);
    const char* symname = get_str(dso, sym->name);

    // Get relocation type.
    const unsigned reloctype = ELF64_R_TYPE(reloc->info);

    // Find symbol address.
    void* symaddr = 0;
    // FIXME: Should relocations of type `R_X86_64_64` only be looked up in `dso` directly?
    if (reloctype == R_X86_64_RELATIVE) {
        // Symbols address is computed by re-basing the relative address based
        // on the DSOs base address.
        symaddr = (void*)(dso->base + reloc->addend);
    } else {
        // Special handling of `R_X86_64_COPY` relocations.
        //
        // The `R_X86_64_COPY` relocation type is used in the main program when
        // it references an object provided by a shared library (eg extern
        // declared variable).
        // The static linker will still allocate storage for the external
        // object in the main programs `.bss` section and any reference to the
        // object from the main program are resolved by the static linker to
        // the location in the `.bss` section directly (relative addressing).
        // During runtime, when resolving the `R_X86_64_COPY` relocation, the
        // dynamic linker will copy the initial value from the shared library
        // that actually provides the objects symbol into the location of the
        // main program. References to the object by other shared library are
        // resolved to the location in the main programs `.bss` section.
        //
        // LinkMap:        Relocs:
        //
        // main program    { sym: foo, type: R_X86_64_COPY }
        //      |
        //      v
        //    libso        { sym: foo, type: R_X86_64_GLOB_DAT }
        //                 // Also `foo` is defined in `libso`.
        //
        //                                         libso
        //                                         +-----------+
        //                                         | .text     |
        //       main prog                         |           |  ref
        //       +-----------+                     | ... [foo] |--+
        //       | .text     |   R_X86_64_GLOB_DAT |           |  |
        //  ref  |           |   Patch address of  +-----------+  |
        //    +--| ... [foo] |   foo in .got.      | .got      |  |
        //    |  |           | +------------------>| foo:      |<-+
        //    |  +-----------+ |                   |           |
        //    |  | .bss      | |                   +-----------+
        //    |  |           | /                   | .data     |
        //    +->| foo: ...  |<--------------------| foo: ...  |
        //       |           | R_X86_64_COPY       |           |
        //       +-----------+ Copy initial value. +-----------+
        //
        // The handling of `R_X86_64_COPY` relocation assumes that the main
        // program is always the first entry in the link map.
        for (const LinkMap* lmap = (reloctype == R_X86_64_COPY ? map->next : map); lmap && symaddr == 0; lmap = lmap->next) {
            symaddr = lookup_sym(lmap->dso, symname);
        }
    }
    ERROR_ON(symaddr == 0, "Failed lookup symbol %s while resolving relocations!", symname);

    pfmt("Resolved reloc %s to %p (base %p)\n", reloctype == R_X86_64_RELATIVE ? "<relative>" : symname, symaddr, dso->base);

    // Perform relocation according to relocation type.
    switch (reloctype) {
        case R_X86_64_GLOB_DAT:  /* GOT entry for data objects. */
        case R_X86_64_JUMP_SLOT: /* PLT entry. */
        case R_X86_64_64:        /* 64bit relocation (non-lazy). */
        case R_X86_64_RELATIVE:  /* DSO base relative relocation. */
            // Patch storage unit of relocation with absolute address of the symbol.
            *(uint64_t*)(dso->base + reloc->offset) = (uint64_t)symaddr;
            break;
        case R_X86_64_COPY: /* Reference to global variable in shared ELF file. */
            // Copy initial value of variable into relocation address.
            memcpy(dso->base + reloc->offset, (void*)symaddr, sym->size);
            break;
        default:
            ERROR_ON(true, "Unsupported relocation type %d!\n", reloctype);
    }
}

// Resolve all relocations of `dso`.
//
// Resolve relocations from the PLT & RELA tables. Use `map` as link map which
// defines the order of the symbol lookup.
static void resolve_relocs(const Dso* dso, const LinkMap* map) {
    // Resolve all relocation from the RELA table found in `dso`. There is
    // typically one relocation per undefined dynamic object symbol (eg global
    // variables).
    for (unsigned long relocidx = 0; relocidx < (dso->dynamic[DT_RELASZ] / sizeof(Elf64Rela)); ++relocidx) {
        const Elf64Rela* reloc = get_reloca(dso, relocidx);
        resolve_reloc(dso, map, reloc);
    }

    // Resolve all relocation from the PLT jump table found in `dso`. There is
    // typically one relocation per undefined dynamic function symbol.
    for (unsigned long relocidx = 0; relocidx < (dso->dynamic[DT_PLTRELSZ] / sizeof(Elf64Rela)); ++relocidx) {
        const Elf64Rela* reloc = get_pltreloca(dso, relocidx);
        resolve_reloc(dso, map, reloc);
    }
}

// }}}
// {{{ Dynamic Linking (lazy resolve)

// Dynamic link handler for lazy resolve.
// This handler is installed in the GOT[2] entry of `Dso` objects which holds
// the address of the jump target for the PLT0 jump pad.
//
// Mark `dynresolve_entry` as `naked` because we don't want a prologue/epilogue
// being generated so we have full control over the stack layout.
//
// `noreturn`  Function never returns.
// `naked`     Don't generate prologue/epilogue sequences.
__attribute__((noreturn)) __attribute__((naked)) static void dynresolve_entry() {
    asm("dynresolve_entry:\n\t"
        // Pop arguments of PLT0 from the stack into rdi/rsi registers
        // These are the first two integer arguments registers as defined by
        // the SystemV abi and hence will be passed correctly to `dynresolve`.
        "pop %rdi\n\t"  // GOT[1] entry (pushed by PLT0 pad).
        "pop %rsi\n\t"  // Relocation index (pushed by PLT0 pad).
        "jmp dynresolve");
}

// `used`    Force to emit code for function.
// `unused`  Don't warn about unused function.
__attribute__((used)) __attribute__((unused)) static void dynresolve(uint64_t got1, uint64_t reloc_idx) {
    ERROR_ON(true,
             "ERROR: dynresolve request not supported!"
             "\n\tGOT[1]    = 0x%x"
             "\n\treloc_idx = %d\n",
             got1, reloc_idx);
}

// }}}
// {{{ Setup GOT

static void setup_got(const Dso* dso) {
    // GOT entries {0, 1, 2} have special meaning for the dynamic link process.
    //   GOT[0]     Hold address of dynamic structure referenced by `_DYNAMIC`.
    //   GOT[1]     Argument pushed by PLT0 pad on stack before jumping to GOT[2],
    //              can be freely used by dynamic linker to identify the caller.
    //   GOT[2]     Jump target for PLT0 pad when doing dynamic resolve (lazy).
    //
    // We will not make use of GOT[0]/GOT[1] here but only GOT[2].

    // Install dynamic resolve handler. This handler is used when binding
    // symbols lazy.
    //
    // The handler is installed in the `GOT[2]` entry for each DSO object that
    // has a GOT. It is jumped to from the `PLT0` pad with the following two
    // arguments passed via the stack:
    //   - GOT[1] entry.
    //   - Relocation index.
    //
    // This can be seen in the following disassembly of section .plt:
    //   PLT0:
    //     push   QWORD PTR [rip+0x3002]        # GOT[1]
    //     jmp    QWORD PTR [rip+0x3004]        # GOT[2]
    //     nop    DWORD PTR [rax+0x0]
    //
    //   PLT1:
    //     jmp    QWORD PTR [rip+0x3002]        # GOT[3]; entry for <PLT1>
    //     push   0x0                           # Relocation index
    //     jmp    401000 <PLT0>
    //
    // The handler at GOT[2] can pop the arguments as follows:
    //    pop %rdi  // GOT[1] entry.
    //    pop %rsi  // Relocation index.

    if (dso->dynamic[DT_PLTGOT] != 0) {
        uint64_t* got = (uint64_t*)(dso->base + dso->dynamic[DT_PLTGOT]);
        got[2] = (uint64_t)&dynresolve_entry;
    }
}

// }}}

// {{{ Dynamic Linker Entrypoint

void dl_entry(const uint64_t* prctx) {
    // Parse SystemV ABI block.
    const SystemVDescriptor sysv_desc = get_systemv_descriptor(prctx);

    // Ensure hard-coded page size value is correct.
    ERROR_ON(sysv_desc.auxv[AT_PAGESZ] != PAGE_SIZE, "Hard-coded PAGE_SIZE miss-match!");

    // Initialize dso handle for user program but extracting necesarry
    // information from `AUXV` and the `PHDR`.
    const Dso dso_prog = get_prog_dso(&sysv_desc);

    // Map dependency.
    //
    // In this chapter the user program should have a single shared
    // object dependency, which is our `libgreet.so` no-std shared
    // library.
    // The `libgreet.so` library itself should not have any dynamic
    // dependencies.
    ERROR_ON(dso_prog.needed_len != 1, "User program should have exactly one dependency!");

    const Dso dso_lib = map_dependency(get_str(&dso_prog, dso_prog.needed[0]));
    ERROR_ON(dso_lib.needed_len != 0, "The library should not have any further dependencies!");

    // Setup LinkMap.
    //
    // Create a list of DSOs as link map with the following order:
    //   main -> libgreet.so
    // The link map determines the symbol lookup order.
    const LinkMap map_lib = {.dso = &dso_lib, .next = 0};
    const LinkMap map_prog = {.dso = &dso_prog, .next = &map_lib};

    // Resolve relocations of the library (dependency).
    resolve_relocs(&dso_lib, &map_prog);
    // Resolve relocations of the main program.
    resolve_relocs(&dso_prog, &map_prog);

    // Initialize library.
    init(&dso_lib);
    // Initialize main program.
    init(&dso_prog);

    // Setup global offset table (GOT).
    //
    // This installs a dynamic resolve handler, which should not be called in
    // this example as we resolve all relocations before transferring control
    // to the user program.
    // For safety we still install a handler which will terminate the program
    // once it is called. If we wouldn't install this handler the program would
    // most probably SEGFAULT in case symbol binding would be invoked during
    // runtime.
    setup_got(&dso_lib);
    setup_got(&dso_prog);

    // Transfer control to user program.
    dso_prog.entry();

    // Finalize main program.
    fini(&dso_prog);
    // Finalize library.
    fini(&dso_lib);

    _exit(0);
}

// }}}

// vim:fdm=marker
