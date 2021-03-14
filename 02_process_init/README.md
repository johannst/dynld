# Process Initialization

### Goals
- Understand initial process state on process entry as specified by the
  [SystemV x86-64 ABI][sysv_x86_64].
- Build a `no-std` program to analyze & visualize the initial process state.

---

Before starting to implement a minimal dynamic linker the first step is to
understand the `process initialization` procedure.
This is important because when starting a `dynamically-linked`
executable the control is first passed to the dynamic linker
(interpreter) by the Linux Kernel as mentioned in
[01_dynamic_linking](../01_dynamic_linking/README.md).

Once the dynamic linker is executing it needs to prepare the execution
environment for the dynamically-linked executable. The dynamic linker's main tasks are:
- To load dependencies.
- Perform re-locations.
- Run initialization routines.

After the execution environment is prepared the dynamic linker hands
control to the user executable.

Due to all this requirements the dynamic must be a free-standing
executable with no dependencies.

## Stack state on process entry

When launching an ELF executable the Linux Kernel will map in the
memory segments from the ELF file and setup some data on the `stack`
according to the specification in the [SystemV x86-64 ABI][sysv_x86_64]
chapter _Initial Stack and Register State_.

On process startup after `execve(2)` the stack looks as follows
```text
        +------------+ High Address
        | ..         |
        | ENV strs   |<-+
     +->| ARG strs   |  |
     |  | ..         |  |
     |  +------------+  |
     |  | ..         |  |
     |  +------------+  |
     |  | AT_NULL    |  |
     |  +------------+  |
     |  | AUXV       |  |
     |  +------------+  |
     |  | 0x0        |  |
     |  +------------+  |
     |  | ENVP       |--+
     |  +------------+
     |  | 0x0        |
     |  +------------+
     +--| ARGV       |
        +------------+
 $rsp ->| ARGC       |
        +------------+ Low Address


     | Offset (in bytes)     | Type                   | Description
-----+-----------------------+------------------------+--------------------
AUXV | &ENVP + 8*#ENVP + 8   | struct { uint64_t[2] } | Auxiliary Vector
 0x0 | &ENVP + 8*#ENVP       |                        | 0 terinator (ENVP)
ENVP | &ARGV + 8*ARGC + 8    | const char* []         | Environment ptrs
 0x0 | &ARGV + 8*ARGC        |                        | 0 terinator (ARGV)
ARGV | $rsp + 8              | const char* []         | Argument ptrs
ARGC | $rsp                  | uint64_t               | Argument count
```

- `ARGV : const char* []` is an array of pointers to string literals
  holding the command line arguments.
  - `ARGV[0]` is special as it holds the path of the launched program.
- `ARGC : uint64_t` is the number of command line arguments + 1
- `ENVP : const char* []` is an array of pointers to string literals
  holding the environment variables as seen by this process
- `AUXV : uint64_t[2]` is the `auxiliary vector` providing additional
  information like the `entry point` or the `program header` of the
  program.

The `AUXV` segment consists of consecutive `AuxvEntry` elements terminated by the `DT_NULL` element.
```c
struct AuxvEntry {
  uint64_t tag;
  uint64_t val;
};
```
The _Auxiliary Vector_ chapter in the [`x86-64 System V ABI`][sysv_x86_64] specifies
the following tags:
```text
AT_NULL   =  0
AT_IGNORE =  1
AT_EXECFD =  2
AT_PHDR   =  3
AT_PHENT  =  4
AT_PHNUM  =  5
AT_PAGESZ =  6
AT_BASE   =  7
AT_FLAGS  =  8
AT_ENTRY  =  9
AT_NOTELF = 10
AT_UID    = 11
AT_EUID   = 12
AT_GID    = 13
AT_EGID   = 14
```
Where `AT_NULL` is used to indicate the end of `AUXV`.

## Register state on process entry

Regarding the state of general purpose registers on process entry the
[x86-64 SystemV ABI][sysv_x86_64] states that all registers except the ones listed
below are in an unspecified state:
- `$rbp`: content is unspecified, but user code should set it to zero to mark
  the deepest stack frame
- `$rsp`: points to the beginning of the data block provided by the Kernel and
  is guaranteed to be 16-byte aligned at process entry
- `$rdx`: function pointer that the application should register with
  `atexit(BA_OS)`.
> Not sure here if clearing `$rbp` is strictly required as frame-pointer
> chaining is optional and can be omitted (`gcc -fomit-frame-pointer`).

## Hands-on the first instruction

Before exploring and visualizing the data passed by the Linux Kernel on the
stack there is one more question to answer:
**How to run the first instruction in a process?**

Typically when building a `C` program the users entry point is the `main`
function, however this won't contain the first instruction executed after the
process entry. This can be seen by extracting the `entry point` from the ELF
header and checking against the symbols in the program. Here the entry point is
`0x1020` which belongs to the symbol `_start` and not `main`.
```bash
readelf -h main | grep Entry
  Entry point address:               0x1020

nm main | grep '1020\|main'
  0000000000001119 T main
  0000000000001020 T _start
```

This is because by default the `static linker` adds some extra code & libraries
to the program like for example the `libc` and the `C-runtime (crt)` which
contains the `_start` symbol and hence the first instruction executed.

Passing `--trace` down to the `static linker` sheds some light onto which
input files the static linker actually processes.
```bash
echo 'void main() {}' | gcc -x c -o /dev/null - -Wl,--trace
/usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/../../../../lib/Scrt1.o
/usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/../../../../lib/crti.o
/usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/crtbeginS.o
/tmp/ccjZdjYx.o
/usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/libgcc.a
/usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/../../../../lib/libgcc_s.so
/usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/../../../../lib/libc.so
/usr/lib/ld-linux-x86-64.so.2
/usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/crtendS.o
/usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/../../../../lib/crtn.o
```
> `/tmp/ccjZdjYx.o` is a temporary file created by the compiler containing the
> code echoed.

The static linker can be explicitly told to not include any default files by
using the `gcc -nostdlib` argument.
```bash
echo 'void _start() {}' | gcc -x c -o /dev/null - -Wl,--trace -nostdlib
/tmp/ccbfkCoZ.o
```
Quoting `man gcc`
> `-nostdlib` Do not use the standard system startup files or libraries when linking.

## Examining the data from the Kernel

With the capability to control the first instruction executed after process
entry we finally can visualize the data passed by the Linux Kernel on the stack.

First we provide the symbol `_start` (default entry point) which saves a
pointer to the Kernel data in `$rdi` and jumps to a function called `entry`.
The pointer is saved in `$rdi` because that's the register for the first
argument of class `INTEGER` ([SystemV ABI Function Arugments][sysv_x86_64_fnarg]).
```asm
.section .text, "ax", @progbits
.global _start
_start:
  // Clear $rbp.
  xor rbp, rbp

  // Load ptr to Kernel data.
  lea rdi, [rsp]

  call entry
  ...
```
The full source code of the `_start` function is available in [entry.S](./entry.S).

The pointer passed to the `entry` function can be used to compute `ARGC`,
`ARGV` and `ENVP` accordingly.
```c
void entry(uint64_t* prctx) {
  uint64_t argc = *prctx;
  const char** argv = (const char**)(prctx + 1);
  const char** envv = (const char**)(argv + argc + 1);
  ...
```

To collect the `AUXV` entries we first need to count the number of environment
variables as follows.
```c
// entry.c
  ...
  int envc = 0;
  for (const char** env = envv; *env; ++env) {
      ++envc;
  }

  uint64_t auxv[AT_MAX_CNT];
  for (unsigned i = 0; i < AT_MAX_CNT; ++i) {
      auxv[i] = 0;
  }

  const Auxv64Entry* auxvp = (const Auxv64Entry*)(envv + envc + 1);
  for (; auxvp->tag != AT_NULL; ++auxvp) {
      if (auxvp->tag < AT_MAX_CNT) {
          auxv[auxvp->tag] = auxvp->val;
      }
  }
  ...
```

Finally the data can be printed as
```c
// entry.c
  ...
  pfmt("Got %d arg(s)\n", argc);
  for (const char** arg = argv; *arg; ++arg) {
      pfmt("\targ = %s\n", *arg);
  }

  const int max_env = 10;
  pfmt("Print first %d env var(s)\n", max_env - 1);
  for (const char** env = envv; *env && (env - envv < max_env); ++env) {
      pfmt("\tenv = %s\n", *env);
  }

  pfmt("Print auxiliary vector\n");
  pfmt("\tAT_EXECFD: %ld\n", auxv[AT_EXECFD]);
  pfmt("\tAT_PHDR  : %p\n", auxv[AT_PHDR]);
  pfmt("\tAT_PHENT : %ld\n", auxv[AT_PHENT]);
  pfmt("\tAT_PHNUM : %ld\n", auxv[AT_PHNUM]);
  pfmt("\tAT_PAGESZ: %ld\n", auxv[AT_PAGESZ]);
  pfmt("\tAT_BASE  : %lx\n", auxv[AT_BASE]);
  pfmt("\tAT_FLAGS : %ld\n", auxv[AT_FLAGS]);
  pfmt("\tAT_ENTRY : %p\n", auxv[AT_ENTRY]);
  pfmt("\tAT_NOTELF: %lx\n", auxv[AT_NOTELF]);
  pfmt("\tAT_UID   : %ld\n", auxv[AT_UID]);
  pfmt("\tAT_EUID  : %ld\n", auxv[AT_EUID]);
  pfmt("\tAT_GID   : %ld\n", auxv[AT_GID]);
  pfmt("\tAT_EGID  : %ld\n", auxv[AT_EGID]);
  ...
```
The full source code of the `entry` function is available in [entry.c](./entry.c).

Running the program as `./entry 1 2 3 4` it yields following output:
```text
Got 5 arg(s)
        arg = ./entry
        arg = 1
        arg = 2
        arg = 3
        arg = 4
Print first 9 env var(s)
        env = I3SOCK=/run/user/1000/i3/ipc-socket.1200
        env = LC_NAME=en_US.UTF-8
        env = LC_NUMERIC=en_US.UTF-8
        env = WINDOWID=46221701
        env = LC_ADDRESS=en_US.UTF-8
        env = GDM_LANG=en_US.utf8
        env = PWD=/home/johannst/dev/dynld/02_process_init
        env = MAIL=/var/spool/mail/johannst
        env = XDG_SESSION_PATH=/org/freedesktop/DisplayManager/Session
Print auxiliary vector
        AT_EXECFD: 0
        AT_PHDR  : 0x400040
        AT_PHENT : 56
        AT_PHNUM : 5
        AT_PAGESZ: 4096
        AT_BASE  : 0
        AT_FLAGS : 0
        AT_ENTRY : 0x401000
        AT_NOTELF: 0
        AT_UID   : 1000
        AT_EUID  : 1000
        AT_GID   : 1000
        AT_EGID  : 1000
```

## Things to remember
- On process entry the Linux Kernel provides data on the stack as specified in
  the [SystemV ABI][sysv_x86_64].
- By default the `static linker` adds additional code which contains the
  `_start` symbol being the default process `entry point`.

## References & Source Code
- [x86-64 SystemV ABI][sysv_x86_64]
- [x86-64 SystemV ABI - Passing arguments to functions][sysv_x86_64_fnarg]
- [entry.S](./entry.S)
- [entry.c](./entry.c)

[sysv_x86_64]: https://www.uclibc.org/docs/psABI-x86_64.pdf
[sysv_x86_64_fnarg]: https://johannst.github.io/notes/arch/x86_64.html#passing-arguments-to-functions
