# Hello `dynld`

### Goals
- Build dynamic linker `dynld.so` which retrieves the user program's
  entrypoint (`AT_ENTRY`) from the auxiliary vector and transfers
  control to it.
- Build `no-std` program with a custom `PT_INTERP` entry pointing to
  `dynld.so`.

---

## Crafting the `dynld.so`

As described in the `goals` above, the idea in this section is to
create a simple dynamic linker which just gets the `entrypoint` of the
user application and then jumps to it. This means the linker does not
support things like:
- Loading of additional dependencies.
- Performing re-locations.

That said, this dynamic linker will not be particularly useful but
act as a skeleton for the upcoming chapters.

The `entrypoint` of the user executable started by the dynamic linker
can be found in the `auxiliary vector` setup by the Linux Kernel (see
[02_process_init](../02_process_init/README.md)).
The entry of interest is the `AT_ENTRY`:
```text
AT_ENTRY
  The `a_ptr` member of this entry holds the entry point of
  the application program to which the interpreter program should
  transfer control.
```

There are two additional entries that need to be discussed,
`AT_EXECFD` and `AT_PHDR`. The x86_64 SystemV ABI states that the OS
Kernel must provide one or the other in the `auxiliary vector`.
For simplicity the dynamic linker in this section only supports
`AT_PHDR`, which means it requires the OS Kernel to already memory map
the user executable.
```text
AT_EXECFD
  At process creation the system may pass control to an
  interpreter program. When this happens, the system places
  either an entry of type `AT_EXECFD` or one of type `AT_PHDR`
  in the auxiliary vector. The entry for type `AT_EXECFD`
  contains a file descriptor open to read the application
  program’s object file.

AT_PHDR
  The system may create the memory image of the application
  program before passing control to the interpreter
  program. When this happens the `AT_PHDR` entry tells the
  interpreter where to find the program header table in the
  memory image.
```

Using the [`no-std` program](../02_process_init/entry.c) from chapter
[02_process_init](../02_process_init) as starting point, loading and
jumping to the `entrypoint` of the user program can be done as:
```c
void (*user_entry)() = (void (*)())auxv[AT_ENTRY];
user_entry();
```

## User program

The next step is to create the user program that will be loaded by
the dynamic linker created in the previous section.
For now the functionality of the user program is not important, but it
must full-fill the requirements no to depend on any shared libraries or
contain any relocations.
For this purpose the following simple `no-std` program is used:
```c
#include <syscall.h>
#include <io.h>

#include <asm/unistd.h>

void _start() {
    pfmt("Running %s @ %s\n", __FUNCTION__, __FILE__);

    syscall1(__NR_exit, 0);
}
```

The important step, when linking the user program, is to inform the
static linker to add the `dynld.so` created above in the `.interp`
section. This can be done with the following command line switch:
```bash
gcc ... -Wl,--dynamic-linker=<path> ...
```
> The full compile and link command can be seen in the [Makefile - main](./Makefile).

The result can be seen in the `.interp` sections referenced by the
`PT_INTERP` segment in the program headers:
```bash
readelf -W --string-dump .interp main

String dump of section '.interp':
  [     0]  /home/johannst/dev/dynld/03_hello_dynld/dynld.so
```
```bash
readelf -W --program-headers main

Program Headers:
  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
  PHDR           0x000040 0x0000000000000040 0x0000000000000040 0x0002d8 0x0002d8 R   0x8
  INTERP         0x000318 0x0000000000000318 0x0000000000000318 0x000031 0x000031 R   0x1
      [Requesting program interpreter: /home/johannst/dev/dynld/03_hello_dynld/dynld.so]
  ...
```

As discussed in [01_dynamic_linking](../01_dynamic_linking/README.md)
the `PT_INTERP` segment tells to Linux Kernel which dynamic linker to
load to handle the startup of the user executable.

When running the `./main` user program, the `dynld.so` will be loaded
by the Linux Kernel and controlled will be handed over to it. The
`dynld.so` will retrieve the `entrypoint` of the user program and then
jump to it.

## Things to remember
- As defined by the SystemV ABI the OS Kernel must either provide an
  entry for `AT_EXECFD` or `AT_PHDR` in the `auxiliary vector`.
- The `AT_ENTRY` points to the `entrypoint` of the user program.
- When linking with gcc, the dynamic linker can be specified via `-Wl,--dynamic-linker=<path>`.
