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
- Dynamic resolve during runtime (lazy).
- Threads locals storage (TLS).

However with a little effort this dynamic linker could easily be extend.

Before diving into details, let's first define the high-level tasks of
`dynld.so`:
1. Parse out necessary information from the initial process context ([`SystemV`
   context](../02_process_init/README.md#stack-state-on-process-entry)).
1. Map `libgreet.so` shared library dependency.
1. Resolve all relocations of `libgreet.so` and `main`.
1. Run `INIT` functions of `libgreet.so` and `main`.
1. Transfer control to `main` user program.
1. Run `FINI` functions of `libgreet.so` and `main`.

[gcc-fn-attributes]: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes
