# `dynld` no-std

### Goals
- Create a `no-std` shared library `libgreet.so` which exposes some functions
  and variables.
- Create a `no-std` user executable which dynamically links against
  `libgreet.so` and uses exposed functions and variables.
- Create a dynamic linker `dynld.so` which can prepare the execution
  environment, by mapping the shared library dependency and resolving all
  relocations.

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
That information is captured in a `dynamic shared object (dso)` structure:
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

Computing the `base address` can be done by using the `PT_PHDR` program header
which describes the program headers itself. The absolute `base address` is then
computed by subtracting the relative `PT_PHDR->vaddr` from the absolute address
in the `AT_PDHR` entry from the auxiliary vector. Looking at the figure below
this becomes more clearer.
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
`base address` can be see here and the result is stored in the `dso` object
representing the user program.
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
> Tags are defined in [elf.h](../lib/include/elf.h).

With the absolute `base address` of the `dso` the `.dynamic` section can be
located by using the address from the `PT_DYNAMIC->vaddr`. When iterating over
the program headers above, this offset was already stored in `dynoff` and
passed to the `decode_dynamic` function.
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

The last step to extract the info of the user program is to store the address
of the entry function where the dynamic linker will pass control to later. This
can be found in the auxiliary vector in the `AT_ENTRY` entry.
```c
static Dso get_prog_dso(const SystemVDescriptor* sysv) {
    ...
    prog.entry = (void (*)())sysv->auxv[AT_ENTRY];
```

### (2) Map `libgreet.so`
### (3) Resolve relocations
### (4) Run `init` functions
### (5) Run the user program
### (6) Run `fini` functions

[gcc-fn-attributes]: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes
