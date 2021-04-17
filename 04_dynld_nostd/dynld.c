// Copyright (c) 2021 Johannes Stoelp

#include <auxv.h>
#include <common.h>
#include <elf.h>
#include <io.h>
#include <syscalls.h>

#include <stdbool.h>
#include <stdint.h>


/// ----------------
/// Global Constants
/// ----------------

enum {
    // Hard-coded page size.
    // We assert against the `AT_PAGESZ` auxiliary vector entry.
    PAGE_SIZE = 4096,
    // Hard-coded upper limit of `DT_NEEDED` entries per dso
    // (for simplicity to not require allocations).
    MAX_NEEDED = 1,
};


/// --------
/// Execinfo
/// --------

typedef struct {
    uint64_t argc;              // Number of commandline arguments.
    const char** argv;          // List of pointer to command line arguments.
    uint64_t envc;              // Number of environment variables.
    const char** envv;          // List of pointers to environment variables.
    uint64_t auxv[AT_MAX_CNT];  // Auxiliary vector entries.
} ExecInfo;

// Interpret and extract data passed on the stack by the Linux Kernel
// when loading the initial process image.
// The data is organized according to the SystemV x86_64 ABI.
ExecInfo get_exec_info(const uint64_t* prctx) {
    ExecInfo info = {0};

    info.argc = *prctx;
    info.argv = (const char**)(prctx + 1);
    info.envv = (const char**)(info.argv + info.argc + 1);

    // Count the number of environment variables in the `ENVP` segment.
    for (const char** env = info.envv; *env; ++env) {
        info.envc += 1;
    }

    // Decode auxiliary vector `AUXV`.
    for (const Auxv64Entry* auxvp = (const Auxv64Entry*)(info.envv + info.envc + 1); auxvp->tag != AT_NULL; ++auxvp) {
        if (auxvp->tag < AT_MAX_CNT) {
            info.auxv[auxvp->tag] = auxvp->val;
        }
    }

    return info;
}


/// ---
/// Dso
/// ---

typedef struct {
    uint8_t* base;                 // Base address.
    void (*entry)();               // Entry function.
    uint64_t dynamic[DT_MAX_CNT];  // `.dynamic` section entries.
    uint64_t needed[MAX_NEEDED];   // Shared object dependencies (`DT_NEEDED` entries).
    uint8_t needed_len;            // Number of `DT_NEEDED` entries (SO dependencies).
} Dso;

void decode_dynamic(Dso* dso, uint64_t dynoff) {
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

    // Check for relocation entries related to PLT.
    // ERROR_ON(dso->dynamic[DT_JMPREL] == 0, "DT_JMPREL missing in dynamic section!");
    // ERROR_ON(dso->dynamic[DT_PLTRELSZ] == 0, "DT_PLTRELSZ missing in dynamic section!");
    // ERROR_ON(dso->dynamic[DT_PLTREL] == 0, "DT_PLTREL missing in dynamic section!");
    // ERROR_ON(dso->dynamic[DT_PLTREL] != DT_RELA, "x86_64 only uses RELA entries!");

    // Check for SystemV hash table. We only support SystemV hash tables
    // `DT_HASH`, not gnu hash tables `DT_GNU_HASH`.
    ERROR_ON(dso->dynamic[DT_HASH] == 0, "DT_HASH missing in dynamic section!");
}

Dso get_prog_info(const ExecInfo* info) {
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
    ERROR_ON(info->auxv[AT_PHDR] == 0 || info->auxv[AT_EXECFD] != 0, "AT_PHDR entry missing in the AUXV!");

    // Offset to the `.dynamic` section from the user programs `base addr`.
    uint64_t dynoff = 0;

    // Program header of the user program.
    const Elf64Phdr* phdr = (const Elf64Phdr*)info->auxv[AT_PHDR];

    ERROR_ON(info->auxv[AT_PHENT] != sizeof(Elf64Phdr), "Elf64Phdr size miss-match!");

    // Decode PHDRs of the user program.
    for (unsigned phdrnum = info->auxv[AT_PHNUM]; --phdrnum; ++phdr) {
        if (phdr->type == PT_PHDR) {
            ERROR_ON(info->auxv[AT_PHDR] < phdr->vaddr, "Expectation auxv[AT_PHDR] >= phdr->vaddr failed!");
            prog.base = (uint8_t*)(info->auxv[AT_PHDR] - phdr->vaddr);
        } else if (phdr->type == PT_DYNAMIC) {
            dynoff = phdr->vaddr;
        }
    }
    ERROR_ON(dynoff == 0, "PT_DYNAMIC entry missing in the user programs PHDR!");

    // Decode `.dynamic` section.
    decode_dynamic(&prog, dynoff);

    // Get the entrypoint of the user program form the auxiliary vector.
    ERROR_ON(info->auxv[AT_ENTRY] == 0, "AT_ENTRY entry missing in the AUXV!");
    prog.entry = (void (*)())info->auxv[AT_ENTRY];

    return prog;
}

uint64_t get_num_dynsyms(const Dso* dso) {
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

const char* get_str(const Dso* dso, const uint64_t idx) {
    ERROR_ON(dso->dynamic[DT_STRSZ] < idx, "String table indexed out-of-bounds!");
    return (const char*)(dso->base + dso->dynamic[DT_STRTAB] + idx);
}

const Elf64Sym* get_sym(const Dso* dso, const uint64_t idx) {
    ERROR_ON(get_num_dynsyms(dso) < idx, "Symbol table index out-of-bounds!");
    return (const Elf64Sym*)(dso->base + dso->dynamic[DT_SYMTAB]) + idx;
}

const Elf64Rela* get_pltreloc(const Dso* dso, const uint64_t idx) {
    ERROR_ON(dso->dynamic[DT_PLTRELSZ] < sizeof(Elf64Rela) * idx, "PLT relocation table indexed out-of-bounds!");
    return (const Elf64Rela*)(dso->base + dso->dynamic[DT_JMPREL]) + idx;
}


/// -------------
/// Symbol lookup
/// -------------

int strcmp(const char* s1, const char* s2) {
    while (*s1 == *s2 && *s1) {
        ++s1;
        ++s2;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void* lookup_sym(const Dso* dso, const char* sym_name) {
    for (unsigned i = 0; i < get_num_dynsyms(dso); ++i) {
        const Elf64Sym* sym = get_sym(dso, i);

        if (ELF64_ST_TYPE(sym->info) == STT_FUNC && ELF64_ST_BIND(sym->info) == STB_GLOBAL && sym->shndx != SHN_UNDEF) {
            if (strcmp(sym_name, get_str(dso, sym->name)) == 0) {
                return dso->base + sym->value;
            }
        }
    }

    return 0;
}


/// -----------------------------
/// Map Shared Library Dependency
/// -----------------------------

Dso map_dependency(const char* dependency) {
    // For simplicity we only search for SO dependencies in the current working dir.
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
    }

    // Align start address to the next lower page boundary.
    addr_start = addr_start & ~(PAGE_SIZE - 1);
    // Align end address to the next higher page boundary.
    addr_end = (addr_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Reserve region big enough to map all `PT_LOAD` sections of `dependency`.
    uint8_t* map = mmap(0 /* addr */, addr_end - addr_start /* len */, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                        -1 /* fd */, 0 /* file offset */);
    ERROR_ON(map == MAP_FAILED, "Failed to mmap address space for dependency '%s'\n", dependency);

    // Compute base address for library.
    uint8_t* base = map - addr_start;

    // Map in all `PT_LOAD` segments from the `dependency`.
    for (unsigned i = 0; i < ehdr.phnum; ++i) {
        const Elf64Phdr* p = &phdr[i];
        ;
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
            memset(base + addr_start + p->filesz, 0 /* byte */, p->memsz - p->filesz /*len*/);
        }
    }

    // Close file descriptor.
    close(fd);

    Dso dso = {0};
    dso.base = base;
    decode_dynamic(&dso, dynoff);
    return dso;
}


/// ------------------------------
/// Dynamic Linking (lazy resolve)
/// ------------------------------

struct LinkMap {
    const Dso* dso;              // Pointer to Dso list object.
    const struct LinkMap* next;  // Pointer to next LinkMap entry.
};
typedef struct LinkMap LinkMap;

void resolve_relocs(const Dso* dso, const LinkMap* map) {
    for (unsigned long relocidx = 0; relocidx < (dso->dynamic[DT_PLTRELSZ] / sizeof(Elf64Rela)); ++relocidx) {
        const Elf64Rela* reloc = get_pltreloc(dso, relocidx);
        ERROR_ON(ELF64_R_TYPE(reloc->info) != R_X86_64_JUMP_SLOT, "Expected relocation entry of type X86_64_JUMP_SLOT!");

        const int symidx = ELF64_R_SYM(reloc->info);
        const char* symname = get_str(dso, get_sym(dso, symidx)->name);

        void* symaddr = 0;
        for (const LinkMap* lmap = map; lmap && symaddr == 0; lmap = lmap->next) {
            symaddr = lookup_sym(lmap->dso, symname);
        }
        ERROR_ON(symaddr == 0, "Failed lookup symbol %s while resolving relocations!", symname);

        pfmt("Resolved reloc %s to %p\n", symname, symaddr);

        // Patch storage unit of relocation with absolute address of the symbol.
        *(uint64_t*)(dso->base + reloc->offset) = (uint64_t)symaddr;
    }
}


/// ------------------------------
/// Dynamic Linking (lazy resolve)
/// ------------------------------

// `noreturn`  Function never returns.
// `naked`     Don't generate prologue/epilogue sequences.
__attribute__((noreturn)) __attribute__((naked)) static void dynresolve_entry() {
    asm("dynresolve_entry:\n\t"
        // Pop arguments on stack from PLT0 into rdi/rsi argument registers.
        "pop %rdi\n\t"  // GOT[1] entry (pushed by PLT0 pad).
        "pop %rsi\n\t"  // Relocation index (pushed by PLT0 pad).
        "jmp dynresolve");
}

// `used`    Foce to emit code for function.
// `unused`  Don't warn about unused function.
__attribute__((used)) __attribute__((unused)) static void dynresolve(uint64_t got1, uint64_t reloc_idx) {
    ERROR_ON(true,
             "ERROR: dynresolve request not supported!"
             "\n\tGOT[1]    = 0x%x"
             "\n\treloc_idx = %d\n",
             got1, reloc_idx);
}


/// -------------------------
/// Dynamic Linker Entrypoint
/// -------------------------

void dl_entry(const uint64_t* prctx) {
    // Parse SystemV ABI block.
    const ExecInfo exec_info = get_exec_info(prctx);

    // Ensure hard-coded page size value is correct.
    ERROR_ON(exec_info.auxv[AT_PAGESZ] != PAGE_SIZE, "Hard-coded PAGE_SIZE miss-match!");

    // Initialize dso handle for user program but extracting necesarry
    // information from `AUXV` and the `PHDR`.
    const Dso dso_prog = get_prog_info(&exec_info);

    // Map dependency.
    //
    // In this chapter the user program should have a single shared
    // object dependency, which is our `libgreet.so` no-std shared
    // library.
    // The `libgreet.so` library itself should not have any dynamic
    // dependencies.
    ERROR_ON(dso_prog.needed_len != 1, "User program should have exactly one dependency!");

    const Dso dso_lib = map_dependency(get_str(&dso_prog, dso_prog.needed[0]));
    ERROR_ON(dso_lib.needed_len != 0, "The programs dependency should be stand-alone!");

    // Setup LinkMap.
    //
    // Create a list of DSOs as link map with the following order:
    //   main -> libgreet.so
    // The link map determines the symbol lookup order.
    const LinkMap map_lib = {.dso = &dso_lib, .next = 0};
    const LinkMap map_prog = {.dso = &dso_prog, .next = &map_lib};

    // Resolve relocations for the main program.
    resolve_relocs(&dso_prog, &map_prog);


    // Install dynamic resolve handler.
    //
    // The dynamic resolve handler is used when binding symbols lazily. Hence
    // it should not be called in this example as we resolve all relocations
    // before transfering controll to the user program.
    // For safety we still install a handler which will terminate the program
    // once it is called, if we wouldn't install this handler the program would
    // most probably SEGFAULT.
    //
    // The handler is installed in the `GOT[2]` entry for each DSO object that
    // has an GOT. It is jumped to from the `PLT0` pad with the following two
    // arguments passed via the stack:
    //    pop %rdi  // GOT[1] entry.
    //    pop %rsi  // Relocation index.
    {
        uint64_t* got = (uint64_t*)(dso_prog.base + dso_prog.dynamic[DT_PLTGOT]);
        // Jump target for PLT0 pad.
        got[2] = (uint64_t)&dynresolve_entry;
    }

    // GOT[0];  // Hold address of dynamic structure referenced by `_DYNAMIC`.
    // GOT[1];  // Pushed by PLT0 pad on stack before jumping to got[2] -> Word the dynamic linker can use to identify the caller.
    // GOT[2];  // Jump target for PLT0 pad (when doing lazy resolving).

    dso_prog.entry();

    _exit(0);
}
