# Hello dynamic linking

In `dynamic linking` a program can use code that is not contained in the
program itself but rather in separate library files, so called shared objects.

A statically linked program contains all the `code` & `data` that it needs to
run from start until completion. The program will be loaded by the OS from the
disk into the virtual address space and control is handed over to the mapped
program.
```text
                            @vm
                         |        |
  @disk                  |--------|
+--------+   execve(2)   |        | <- $rip
| prog A | ------------> | prog A |
+--------+               |        |
                         |--------|
                         |        |
```

A dynamically linked program needs to specify a `dynamic linker` which is
basically a runtime interpreter. The OS will additionally load that interpreter
into the virtual address space and give control to the interpreter rather than
the user program.
The interpreter will prepare the execution environment, like loading the
dependencies and so on and once that is done pass control to the user program.
```text
                                  @vm                      @vm
                               |        |               |          |
  @disk                        |--------|               |----------|
+--------------+   execve(2)   |        |               |          | <- $rip
| prog A       | ------------> | prog A |               | prog A   |
+--------------+               |        |   load deps   |          |
| interp ldso  |               |--------| ------------> |----------|
| dep libgreet |               |        |               |          |
+--------------+               |--------|               |----------|
                               | ldso   | <- $rip       | ldso     |
                               |--------|               |----------|
                                                        |          |
                                                        |----------|
                                                        | libgreet |
                                                        |----------|
```
> NOTE: Technically the OS does not need to load the user program itself in
> case it is dynamically linked, but that detail is not important here.

In `ELF` files the name of the dynamic linker is specified in the `.interp` section.
```bash
readelf -W --string-dump .interp main

String dump of section '.interp':
  [     0]  /lib64/ld-linux-x86-64.so.2
```

The `.interp` section is referenced by the `PT_INTERP` segment in the program
headers. During `execve(2)` in the [`load_elf_binary`][load_elf_binary]
function (Linux Kernel) this segment is used to check if the program needs a
dynamic linker and to get its name.
```bash
readelf -W --sections --program-headers main

Section Headers:
  [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  0
  [ 1] .interp           PROGBITS        00000000000002a8 0002a8 00001c 00   A  0   0  1
  ...

Program Headers:
  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
    PHDR           0x000040 0x0000000000000040 0x0000000000000040 0x000268 0x000268 R   0x8
    INTERP         0x0002a8 0x00000000000002a8 0x00000000000002a8 0x00001c 0x00001c R   0x1
        [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
    ...
```

Using `gdb` to break on the first instruction (`starti`) and printing the
backtrace (`bt`) it can be seen that the control first is passed to the
dynamic linker `ld-linux-x86.so.2` rather than to the user program.
```bash
gdb -q --batch -ex 'starti' -ex 'bt' ./main

Program stopped.
0x00007ffff7fd2090 in _start () from /lib64/ld-linux-x86-64.so.2
#0  0x00007ffff7fd2090 in _start () from /lib64/ld-linux-x86-64.so.2
#1  0x0000000000000001 in ?? ()
#2  0x00007fffffffe43e in ?? ()
#3  0x0000000000000000 in ?? ()
```
> NOTE: Frames `#1`, `#2`, `#3` don't actually exist, gdb's unwinder just tried to further unwind the stack.

## Things to remember
- Dynamically linked programs use code contained in separate library files.
- The `dynamic linker` is an interpreter loaded by the OS and gets control
  before the user program.
- A dynamically linked program specifies the dynamic linker needed in the
  `.interp` ELF section.

[load_elf_binary]: https://elixir.bootlin.com/linux/v5.9.8/source/fs/binfmt_elf.c#L850
