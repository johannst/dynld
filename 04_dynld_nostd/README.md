# `dynld` no-std

### Goals
- Create a `no-std` shared library `libgreet.so` which exposes some functions
  and variables.
- Create a `no-std` user executable which dynamically links against
  `libgreet.so` and uses exposed functions and variables.
- Create a dynamic linker `dynld.so` which can prepare the execution
  environment, by mapping the shared library dependency and resolving all
  relocations.

> In code blocks included in this page, the error checking code is omitted to
> purely focus on the functionality they are trying to show-case.

---

## Creating the shared library `libgreet.so`

To challenge the dynamic linker at least a little bit, the shared library will
contain different functionality to generate different kinds of relocations.

The first part consists of a global variable `gCalled` and a global function
`get_greet`. Since the global variable is referenced in the function and the
variable does not have `internal` linkage, this will generate a relocation in
the shared library object.
```cpp
int gCalled = 0;

const char* get_greet() {
    // Reference global variable -> generates RELA relocation (R_X86_64_GLOB_DAT).
    ++gCalled;
    return "Hello from libgreet.so!";
}
```

Additionally the shared library contains a `constructor` and `destructor`
function which will be added to the `.init_array` and `.fini_array` sections
accordingly. The dynamic linkers task is to run these function during
initialization and shutdown of the shared library.
```cpp
// Definition of `static` function which is referenced from the `DT_INIT_ARRAY`
// dynamic section entry -> generates R_X86_64_RELATIVE relocation.
__attribute__((constructor)) static void libinit() {
    pfmt("libgreet.so: libinit\n");
}

// Definition of `non static` function which is referenced from the
// `DT_FINI_ARRAY` dynamic section entry -> generates R_X86_64_64 relocation.
__attribute__((destructor)) void libfini() {
    pfmt("libgreet.so: libfini\n");
}
```
> `constructor` / `destructor` are function attributes and their definition is
> described in [gcc common function attributes][gcc-fn-attributes].

The generated relocations can be seen in the `readelf` output of the shared
library ELF file.
```bash
> readelf -r libgreet.so

Relocation section '.rela.dyn' at offset 0x3f0 contains 3 entries:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
000000003e88  000000000008 R_X86_64_RELATIVE                    1064
000000003e90  000300000001 R_X86_64_64       000000000000107c libfini + 0
000000003ff8  000400000006 R_X86_64_GLOB_DAT 0000000000004020 gCalled + 0

Relocation section '.rela.plt' at offset 0x438 contains 1 entry:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
000000004018  000100000007 R_X86_64_JUMP_SLO 0000000000000000 pfmt + 0
```

Dumping the `.dynamic` section of the shared library, it can be see that there
are `INIT_*` / `FINI_*` entries. These are generated as result of the
`constructor` / `destructor` functions.
The dynamic linker can make use of those entries at runtime to locate the
`.init_array` / `.fini_array` sections and run the functions accordingly.
```bash
> readelf -d libgreet.so

Dynamic section at offset 0x2e98 contains 18 entries:
  Tag        Type                         Name/Value
 0x0000000000000019 (INIT_ARRAY)         0x3e88
 0x000000000000001b (INIT_ARRAYSZ)       8 (bytes)
 0x000000000000001a (FINI_ARRAY)         0x3e90
 0x000000000000001c (FINI_ARRAYSZ)       8 (bytes)
 -- snip --
 0x0000000000000002 (PLTRELSZ)           24 (bytes)
 0x0000000000000014 (PLTREL)             RELA
 0x0000000000000017 (JMPREL)             0x438
 0x0000000000000007 (RELA)               0x3f0
 0x0000000000000008 (RELASZ)             72 (bytes)
 0x0000000000000009 (RELAENT)            24 (bytes)
 0x0000000000000000 (NULL)               0x0
```

The full source code of the shared library is available in
[libgreet.c](./libgreet.c).

## Creating the user executable

The user program looks as follows, it will just make use of the `libgreet.so`
global variable and functions.
```cpp
// API of `libgreet.so`.
extern const char* get_greet();
extern const char* get_greet2();
extern int gCalled;

void _start() {
    pfmt("Running _start() @ %s\n", __FILE__);

    // Call function from libgreet.so -> generates PLT relocations (R_X86_64_JUMP_SLOT).
    pfmt("get_greet()  -> %s\n", get_greet());
    pfmt("get_greet2() -> %s\n", get_greet2());

    // Reference global variable from libgreet.so -> generates RELA relocation (R_X86_64_COPY).
    pfmt("libgreet.so called %d times\n", gCalled);
}
```

Inspecting the relocations again with `readelf` it can be seen that they
contain entries for the referenced variable and functions of the shared
library.
```bash
> readelf -r main

Relocation section '.rela.dyn' at offset 0x478 contains 1 entry:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
000000404028  000300000005 R_X86_64_COPY     0000000000404028 gCalled + 0

Relocation section '.rela.plt' at offset 0x490 contains 2 entries:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
000000404018  000200000007 R_X86_64_JUMP_SLO 0000000000000000 get_greet + 0
000000404020  000400000007 R_X86_64_JUMP_SLO 0000000000000000 get_greet2 + 0
```

The last important piece is to dynamically link the user program against
`libgreet.so` which will generate a `DT_NEEDED` entry in the `.dynamic`
section.
```bash
> readelf -r -d main

Dynamic section at offset 0x2ec0 contains 15 entries:
  Tag        Type                         Name/Value
 0x0000000000000001 (NEEDED)             Shared library: [libgreet.so]
 -- snip ---
 0x0000000000000000 (NULL)               0x0
```

The full source code of the user program is available in
[main.c](./main.c).

## Creating the dynamic linker `dynld.so`

The dynamic linker developed here is kept simple and mainly used to explore the
mechanics of dynamic linking.  That said, it means that it is tailored
specifically for the previously developed executable and won't support things as
- Multiple shared library dependencies.
- Dynamic symbol resolve during runtime (lazy bindings).
- Passing arguments to the user program.
- Thread locals storage (TLS).

However, with a little effort, this dynamic linker could easily be extend and
generalized more.

Before diving into details, let's first define the high-level structure of
`dynld.so`:
1. Decode initial process state from the stack([`SystemV ABI`
   context](../02_process_init/README.md#stack-state-on-process-entry)).
1. Map the `libgreet.so` shared library dependency.
1. Resolve all relocations of `libgreet.so` and `main`.
1. Run `INIT` functions of `libgreet.so` and `main`.
1. Transfer control to user program `main`.
1. Run `FINI` functions of `libgreet.so` and `main`.

When discussing the dynamic linkers functionality below, it is helpful to
understand and keep the following links between the ELF structures in mind.
- From the `PHDR` the dynamic linker can find the `.dynamic` section.
- From the `.dynamic` section, the dynamic linker can find all information
  required for dynamic linking such as the `relocation table`, `symbol table` and
  so on.
```text
               PHDR
AT_PHDR ----> +------------+
              | ...        |
              |            |        .dynamic
              | PT_DYNAMIC | ----> +-----------+
              |            |       | DT_SYMTAB | ----> [ Symbol Table (.dynsym) ]
              | ...        |       | DT_STRTAB | ----> [ String Table (.dynstr) ]
              +------------+       | DT_RELA   | ----> [ Relocation Table (.rela.dyn) ]
                                   | DT_JMPREL | ----> [ Relocation Table (.rela.plt) ]
                                   | DT_NEEDED | ----> Shared Library Dependency
                                   | ...       |
                                   +-----------+
```

### (1) Decode initial process state from the stack

This step consists of decoding the `SystemV ABI` block on the stack into an
appropriate data structure. The details about this have already been discussed
in [02 Process initialization](../02_process_init/).
```c
typedef struct {
    uint64_t argc;              // Number of commandline arguments.
    const char** argv;          // List of pointer to command line arguments.
    uint64_t envc;              // Number of environment variables.
    const char** envv;          // List of pointers to environment variables.
    uint64_t auxv[AT_MAX_CNT];  // Auxiliary vector entries.
} SystemVDescriptor;

void dl_entry(const uint64_t* prctx) {
    // Parse SystemV ABI block.
    const SystemVDescriptor sysv_desc = get_systemv_descriptor(prctx);
    ...
```

With the SystemV ABI descriptor, the next step is to extract the information of
the user program that are of interest to the dynamic linker. 
That information is captured in a `dynamic shared object (dso)` structure as
defined below.
```c
typedef struct {
    uint8_t* base;                 // Base address.
    void (*entry)();               // Entry function.
    uint64_t dynamic[DT_MAX_CNT];  // `.dynamic` section entries.
    uint64_t needed[MAX_NEEDED];   // Shared object dependencies (`DT_NEEDED` entries).
    uint32_t needed_len;           // Number of `DT_NEEDED` entries (SO dependencies).
} Dso;
```

Filling in the `dso` structure is achieved by following the ELF structures as
shown above. 
First, the address of the program headers can be found in the `AT_PHDR` entry
in the auxiliary vector. From there the `.dynamic` section can be located by
using the program header `PT_DYNAMIC->vaddr` entry.

However before using the `vaddr` field, first the `base address` of the `dso`
needs to be computed. This is important because addresses in the program header
and the dynamic section are relative to the `base address`.

The `base address` can be computed by using the `PT_PHDR` program header which
describes the program headers itself. The absolute `base address` is then
computed by subtracting the relative `PT_PHDR->vaddr` from the absolute address
in the `AT_PDHR` entry from the auxiliary vector. Looking at the figure below
this becomes more clear.
```text
                VMA
                |         |
base address -> |         |  -
                |         |  | <---------------------+
     AT_PHDR -> +---------+  -                       |
                |         |                          |
                | PT_PHDR | -----> Elf64Phdr { .., vaddr, .. }
                |         |
                +---------+
                |         |
```
> For `non-pie` executables the `base address` is typically `0x0`, while for
> `pie` executables it is typically **not** `0x0`.

Looking at the concrete implementation in the dynamic linker, computing the
`base address` is done while iterating over the program headers. The result is
stored in the `dso` object representing the user program.
```c
static Dso get_prog_dso(const SystemVDescriptor* sysv) {
    ...
    const Elf64Phdr* phdr = (const Elf64Phdr*)sysv->auxv[AT_PHDR];
    for (unsigned phdrnum = sysv->auxv[AT_PHNUM]; --phdrnum; ++phdr) {
        if (phdr->type == PT_PHDR) {
            prog.base = (uint8_t*)(sysv->auxv[AT_PHDR] - phdr->vaddr);
        } else if (phdr->type == PT_DYNAMIC) {
            dynoff = phdr->vaddr;
        }
    }
```

Continuing, the next step is to decode the `.dynamic` section.  Entries in the
`.dynamic` section are comprised of `2 x 64bit` words and are interpreted as
follows:
```c
typedef struct {
    uint64_t tag;
    union {
        uint64_t val;
        void* ptr;
    };
} Elf64Dyn;
```
> Available `tags` are defined in [elf.h](../lib/include/elf.h).

The `.dynamic` section is located by using the offset from the
`PT_DYNAMIC->vaddr` entry and adding it to the absolute `base address` of the
`dso`. When iterating over the program headers above, this offset was already
stored in `dynoff` and passed to the `decode_dynamic` function.
```c
static void decode_dynamic(Dso* dso, uint64_t dynoff) {
    for (const Elf64Dyn* dyn = (const Elf64Dyn*)(dso->base + dynoff); dyn->tag != DT_NULL; ++dyn) {
        if (dyn->tag == DT_NEEDED) {
            dso->needed[dso->needed_len++] = dyn->val;
        } else if (dyn->tag < DT_MAX_CNT) {
            dso->dynamic[dyn->tag] = dyn->val;
        }
    }
    ...
```
> The value of `DT_NEEDED` entries contain indexes into the `string table
> (DR_STRTAB)` to get the name of the share library dependency.

The last step to extract the information of the user program is to store the
address of the `entry function` where the dynamic linker will pass control to
once the execution environment is set up.
The address of the `entry function` can be retrieved from the `AT_ENTRY` entry
in the auxiliary vector.
```c
static Dso get_prog_dso(const SystemVDescriptor* sysv) {
    ...
    prog.entry = (void (*)())sysv->auxv[AT_ENTRY];
```

### (2) Map `libgreet.so`

The next step of the dynamic linker is to map the shared library dependency of
the main program. Therefore the value of the `DT_NEEDED` entry in the
`.dynamic` section is used. This entry holds an index into the `string table`
where the name of the dependency can be retrieved from.
```c
static const char* get_str(const Dso* dso, uint64_t idx) {
    return (const char*)(dso->base + dso->dynamic[DT_STRTAB] + idx);
}

void dl_entry(const uint64_t* prctx) {
    ...
    const Dso dso_lib = map_dependency(get_str(&dso_prog, dso_prog.needed[0]));
```
> In this concrete case the main program only has a single shared library
> dependency. However ELF files can have multiple dependencies, in that case
> the `.dynamic` section contains multiple `DT_NEEDED` entries.

The task of the `map_dependency` function now is to iterate over the program
headers of the shared library and map the segments described by each `PT_LOAD`
entry from file system into the virtual address space of the process.

To find the program headers, the first step is to read in the ELF header
because this header contains the file offset and the number of program headers.
This information is then used to read in the program headers from the file.
```c
typedef struct {
    uint64_t phoff;      // Program header file offset.
    uint16_t phnum;      // Number of program header entries.
    ...
} Elf64Ehdr;

static Dso map_dependency(const char* dependency) {
    const int fd = open(dependency, O_RDONLY);

    // Read ELF header.
    Elf64Ehdr ehdr;
    read(fd, &ehdr, sizeof(ehdr);

    // Read Program headers at offset `phoff`.
    Elf64Phdr phdr[ehdr.phnum];
    pread(fd, &phdr, sizeof(phdr), ehdr.phoff);
    ...
```
> Full definition of the `Elf64Ehdr` and `Elf64Phdr` structures are available
> in [elf.h](../lib/include/elf.h).

With the program headers available, the different `PT_LOAD` segments can be
mapped. The strategy here is to first map a whole region in the virtual address
space, big enough to hold all the `PT_LOAD` segments. Once the allocation
succeeded the single `PT_LOAD` segments can be mapped over the allocated
region.

To compute the length of the initial allocation, the `start` and `end` address
must be computed by iterating over all `PT_LOAD` entries and saving the minimal
and maximal address.
After that, the memory region is `mmaped` as private & anonymous mapping with
`address == 0`, telling the OS to choose a virtual address, and `PROT_NONE`
as the `PT_LOAD` segments define their own protection flags.
```c
static Dso map_dependency(const char* dependency) {
    ...
    // Compute start and end address.
    uint64_t addr_start = (uint64_t)-1;
    uint64_t addr_end = 0;
    for (unsigned i = 0; i < ehdr.phnum; ++i) {
        const Elf64Phdr* p = &phdr[i];
        if (p->type == PT_LOAD) {
            if (p->vaddr < addr_start) {
                addr_start = p->vaddr;
            } else if (p->vaddr + p->memsz > addr_end) {
                addr_end = p->vaddr + p->memsz;
            }
        }
    }

    // Page align addresses.
    addr_start = addr_start & ~(PAGE_SIZE - 1);
    addr_end = (addr_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Allocate region big enough to fit all `PT_LOAD` sections.
    uint8_t* map = mmap(0 /* addr */, addr_end - addr_start /* len */,
                        PROT_NONE /* prot */, MAP_PRIVATE | MAP_ANONYMOUS /* flags */,
                        -1 /* fd */, 0 /* file offset */);
```

Now the single `PT_LOAD` segments can be mapped from the ELF file of the shared
library using the open file descriptor `fd` from above.<br>
A segment could contain ELF sections of type `SHT_NOBITS` which contributes to
the segments memory image but don't contain actual data in the ELF file on disk
(typical for `.bss` the zero initialized section). Those sections are normally
at the end of the segment making the `PT_LOAD->memzsz > PT_LOAD->filesz` and
are initialized to `0` during runtime.
```c
static Dso map_dependency(const char* dependency) {
    ...
    // Compute base address for library.
    uint8_t* base = map - addr_start;

    for (unsigned i = 0; i < ehdr.phnum; ++i) {
        const Elf64Phdr* p = &phdr[i];
        if (p->type != PT_LOAD) {
            continue;
        }

        // Page align addresses.
        uint64_t addr_start = p->vaddr & ~(PAGE_SIZE - 1);
        uint64_t addr_end = (p->vaddr + p->memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t off = p->offset & ~(PAGE_SIZE - 1);

        // Compute segment permissions.
        uint32_t prot = (p->flags & PF_X ? PROT_EXEC : 0) |
                        (p->flags & PF_R ? PROT_READ : 0) |
                        (p->flags & PF_W ? PROT_WRITE : 0);

        // Mmap single `PT_LOAD` segment.
        mmap(base + addr_start, addr_end - addr_start, prot, MAP_PRIVATE | MAP_FIXED, fd, off);

        // Initialize trailing length (no allocated in ELF file).
        if (p->memsz > p->filesz) {
            memset(base + p->vaddr + p->filesz, 0 /* byte */, p->memsz - p->filesz /*len*/);
        }
    }
```

With that the shared library dependency is mapped in to the virtual address
space of the user program. The last step is to decode the `.dynamic` section
and initialize the `dso` structure. This is the same as already done for the
user program above and details can be seen in the implementation in
[map_dependency - dynld.c](./dynld.c).

### (3) Resolve relocations

After mapping the shared library the next step is to resolve relocations.
This is the process of resolving references to symbols to actual addresses. For
shared libraries this must be done at runtime rather than static link time as
the `base address` of a shared library is only known at runtime.

One central structure for resolving relocations is the `LinkMap`. This is a
linked list of `dso` objects which defines the order in which `dso` objects are
used when performing symbol lookup.

```c
typedef struct LinkMap {
    const Dso* dso;              // Pointer to Dso list object.
    const struct LinkMap* next;  // Pointer to next LinkMap entry ('0' terminates the list).
} LinkMap;
```

In this implementation the `LinkMap` is setup as follows `main -> libgreet.so`,
meaning that symbols are first looked up in `main` and only if they are not
found, `libgreet.so` will be searched.
```c
void dl_entry(const uint64_t* prctx) {
    ...
    const LinkMap map_lib = {.dso = &dso_lib, .next = 0};
    const LinkMap map_prog = {.dso = &dso_prog, .next = &map_lib};
```

With the `LinkMap` setup the `dynld.so` can start processing relocations of the
main program and the shared library. The dynamic linker will process the
following two relocation tables for all `dso` objects on startup:
- `DT_RELA`: Relocations that **must** be resolved during startup.
- `DT_JMPREL`: Relocations associated with the procedure linkage table (those
  could be resolved lazily during runtime, but here they are directly resolved
  during startup).

```c
static void resolve_relocs(const Dso* dso, const LinkMap* map) {
    for (unsigned long relocidx = 0; relocidx < (dso->dynamic[DT_RELASZ] / sizeof(Elf64Rela)); ++relocidx) {
        const Elf64Rela* reloc = get_reloca(dso, relocidx);
        resolve_reloc(dso, map, reloc);
    }

    for (unsigned long relocidx = 0; relocidx < (dso->dynamic[DT_PLTRELSZ] / sizeof(Elf64Rela)); ++relocidx) {
        const Elf64Rela* reloc = get_pltreloca(dso, relocidx);
        resolve_reloc(dso, map, reloc);
    }
}
```

The x86_64 SystemV ABI states that x86_64 only uses `RELA` relocation entries,
which are defined as:
```c
typedef struct {
    uint64_t offset;  // Virtual address of the storage unit affected by the relocation.
    uint64_t info;    // Symbol table index + relocation type.
    int64_t addend;   // Constant value used to compute the relocation value.
} Elf64Rela;
```
So each relocation entry provides the following information required to perform
the relocation
- Virtual address of the storage unit that is affected by the relocation. This
  is the address in memory where the actual address of the resolved symbol will
  be stored to. It is encoded in the `Elf64Rela->offset` field.
- The symbol that needs to be looked up to resolve the relocation. The
  **upper** 32 bit of the `Elf64Rela->info` encode the index into the symbol
  table.
- The relocation type which describes how the relocation should be performed in
  detail. It is encoded in the **lower** 32 bit of the `Elf64Rela->info` field.

The x86_64 SystemV ABI defines many relocation types. As an example, the
following two sub-sections will discuss the relocation types
`R_X86_64_JUMP_SLOT` and `R_X86_64_COPY`.

#### Example: Resolving `R_X86_64_JUMP_SLOT` relocation from `DT_JMPREL` table

Relocation of type `R_X86_64_JUMP_SLOT` are used for entries related to the
`procedure linkage table (PLT)` which is used for function calls between `dso`
objects. This can be seen here, as the main program calls for example the
`get_greet` function provided by the `libgreet.so` shared library which creates
such a relocation entry.
```bash
> readelf -r  main libgreet.so
...

Relocation section '.rela.plt' at offset 0x490 contains 2 entries:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
000000404018  000200000007 R_X86_64_JUMP_SLO 0000000000000000 get_greet + 0
000000404020  000400000007 R_X86_64_JUMP_SLO 0000000000000000 get_greet2 + 0
```

To resolve relocations of this type the following steps need to be performed:
1. Extract the name of the symbol from the relocation entry.
1. Find the address of the symbol by walking the `LinkMap` and searching for
   the symbol.
1. Patch the affected address of the relocation entry with the address of the
   symbol.

The code block below shows a simplified version of the `resolve_reloc` function
which only shows lines that are important for handling relocations of type.
`R_X86_64_JUMP_SLOT`.
```c
static void resolve_reloc(const Dso* dso, const LinkMap* map, const Elf64Rela* reloc) {
    // Get symbol information.
    const int symidx = ELF64_R_SYM(reloc->info);
    const Elf64Sym* sym = get_sym(dso, symidx);
    const char* symname = get_str(dso, sym->name);

    // Get relocation type.
    const unsigned reloctype = ELF64_R_TYPE(reloc->info);
    // assume reloctype == R_X86_64_JUMP_SLOT

    // Lookup address of symbol.
    void* symaddr = 0;
    for (const LinkMap* lmap = map->next; lmap && symaddr == 0; lmap = lmap->next) {
        symaddr = lookup_sym(lmap->dso, symname);
    }

    // Patch address affected by the relocation.
    *(uint64_t*)(dso->base + reloc->offset) = (uint64_t)symaddr;
}
```
> The full implementation of the `resolve_reloc` function can be reviewed in
> [resolve_reloc - dynld.c](./dynld.c).

#### Example: Resolving `R_X86_64_COPY` relocation from `DT_RELA` table

Relocations of type `R_X86_64_COPY` are used in the main program when referring
to an external object provided by a shared library, as for example a global
variable. Here the main program makes use the global variable `extern int
gCalled;` defined in the `libgreet.so` which creates relocations as shown
in the `readelf` dump below.
```bash
> readelf -r  main libgreet.so

File: main

Relocation section '.rela.dyn' at offset 0x478 contains 1 entry:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
000000404028  000300000005 R_X86_64_COPY     0000000000404028 gCalled + 0

...

File: libgreet.so

Relocation section '.rela.dyn' at offset 0x3f0 contains 3 entries:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
...
000000003ff8  000400000006 R_X86_64_GLOB_DAT 0000000000004020 gCalled + 0

...
```

For relocations of this type, the static linker allocates space for the
external symbol in the main programs `.bss` sections.
```bash
> objdump -M intel -d -j .bss main

main:     file format elf64-x86-64

Disassembly of section .bss:

0000000000404028 <gCalled>:
  404028:   00 00 00 00
```

Any reference to the symbol from within the main program is directly resolved
during static link time into the `.bss` section.
```bash
> objdump -M intel -d main

main:     file format elf64-x86-64

Disassembly of section .text:

0000000000401030 <_start>:
  ...
  401088:   8b 05 9a 2f 00 00       mov    eax,DWORD PTR [rip+0x2f9a]        # 404028 <gCalled>
  ...
```

The `R_X86_64_COPY` relocation instructs the dynamic linker now to copy the
initial value from the shared library that provides it into the allocated space
in the main programs `.bss` section.

Shared libraries on the other hand that also reference the same symbol will go
though a `GOT` entry that is patched by the dynamic linker to point to the
location in the `.bss` section of the main program.
Below this can be seen by the `mov` instruction at address `1024` that the
relative address `3ff8` is dereferenced to get the value of the `gCalled`
variable. In the `readelf` dump above it can be seen that there is a relocation
of type `R_X86_64_GLOB_DAT` for symbol `gCalled` affecting the relative address
`3ff8` in the shared library.
```bash
> objdump -M intel -d -j .text -j .got libgreet.so

libgreet.so:     file format elf64-x86-64

Disassembly of section .text:

0000000000001020 <get_greet>:
    1020:   55                      push   rbp
    1021:   48 89 e5                mov    rbp,rsp
    1024:   48 8b 05 cd 2f 00 00    mov    rax,QWORD PTR [rip+0x2fcd]        # 3ff8 <gCalled-0x28>

...

Disassembly of section .got:

0000000000003ff8 <.got>:
    ...
```

The following figure visualizes the described layout above in some more detail.
```text
                                       libso
                                       +-----------+
                                       | .text     |
     main prog                         |           |  ref
     +-----------+                     | ... [foo] |--+
     | .text     |   R_X86_64_GLOB_DAT |           |  |
ref  |           |   Patch address of  +-----------+  |
  +--| ... [foo] |   foo in .got.      | .got      |  |
  |  |           | +------------------>| foo:      |<-+
  |  +-----------+ |                   |           |
  |  | .bss      | |                   +-----------+
  |  |           | /                   | .data     |
  +->| foo: ...  |<--------------------| foo: ...  |
     |           | R_X86_64_COPY       |           |
     +-----------+ Copy initial value. +-----------+
```

To resolve relocations of type `R_X86_64_COPY` the following steps need to be
performed:
1. Extract the name of the symbol from the relocation entry.
1. Find the address of the symbol by walking the `LinkMap` and searching for
   the symbol and excluding the symbol table of the main program `dso`.
1. Copy over the initial value of the symbol into the affected address of the
   relocation entry (`.bss` section of the main program).

The code block below shows a simplified version of the `resolve_reloc` function
which only shows lines that are important for handling relocations of type.
```c
static void resolve_reloc(const Dso* dso, const LinkMap* map, const Elf64Rela* reloc) {
    // Get symbol information.
    const int symidx = ELF64_R_SYM(reloc->info);
    const Elf64Sym* sym = get_sym(dso, symidx);
    const char* symname = get_str(dso, sym->name);

    // Get relocation type.
    const unsigned reloctype = ELF64_R_TYPE(reloc->info);
    // assume reloctype == R_X86_64_COPY

    // Lookup address of symbol.
    void* symaddr = 0;
    for (const LinkMap* lmap = (reloctype == R_X86_64_COPY ? map->next : map); lmap && symaddr == 0; lmap = lmap->next) {
        symaddr = lookup_sym(lmap->dso, symname);
    }

    // Copy initial value of variable into address affected by the relocation.
    memcpy(dso->base + reloc->offset, (void*)symaddr, sym->size);
}
```
> The full implementation of the `resolve_reloc` function can be reviewed in
> [resolve_reloc - dynld.c](./dynld.c).

### (4) Run `init` functions

The next step before transferring control to the main program is to run all the
`init` functions for the `dso` objects. Examples for those are global
`constructors`.
```c
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

void dl_entry(const uint64_t* prctx) {
    ...
    // Initialize library.
    init(&dso_lib);
    // Initialize main program.
    init(&dso_prog);
    ...
}
```

### (5) Run the user program

At that point the execution environment is setup and control can be transferred
from the dynamic linker to the main program.
```c
void dl_entry(const uint64_t* prctx) {
    ...
    // Transfer control to user program.
    dso_prog.entry();
    ...
}
```

### (6) Run `fini` functions

After the main program returned and before terminating the process all the
`fini` functions for the `dso` objects are executed.  Examples for those are
global `destructors`.
```c
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

void dl_entry(const uint64_t* prctx) {
    ...
    // Finalize main program.
    fini(&dso_prog);
    // Finalize library.
    fini(&dso_lib);
    ...
}
```

[gcc-fn-attributes]: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes
