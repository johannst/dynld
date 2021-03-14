# Hello dynamic linking

In `dynamic linking` a program can use code that is not contained in the
program file itself but rather in separate library files, so called shared
objects.

In comparison a statically linked program contains all the `code` & `data` that
it needs to run from start until completion. The program will be loaded by the
Linux Kernel from the disk into the virtual address space and control is handed
over to the mapped program which then executes.
```text
                          @vm
                         |        |
 @disk                   |--------|
+--------+   execve(2)   |        | <- $rip
| prog A | ------------> | prog A |
+--------+               |        |
                         |--------|
                         |        |
```

A dynamically linked program on the other hand needs to specify a `dynamic
linker` which is basically a runtime interpreter. The Linux Kernel will
additionally load that interpreter into the virtual address space and give
control to the interpreter rather than the user program.
The interpreter will prepare the execution environment for the user program
and pass control to it afterwards.
Typical tasks of the interpreter are:
- Loading shared objects into memory (dependencies).
- Performing re-location.
- Running initialization routines.
```text
                                @vm                      @vm
                               |        |               |          |
 @disk                         |--------|               |----------|
+--------------+   execve(2)   |        |               |          | <- $rip
| prog A       | ------------> | prog A |               | prog A   |
+--------------+               |        |   load deps   |          |
| interp ldso  |               |--------| ------------> |----------|
+--------------+               |        |               |          |
| dep libgreet |               |--------|               |----------|
+--------------+               | ldso   | <- $rip       | ldso     |
                               |--------|               |----------|
                                                        |          |
                                                        |----------|
                                                        | libgreet |
                                                        |----------|
```
> NOTE: Technically the Linux Kernel does not need to load the dynamically
> linked user program itself, but that detail is not important here.

In the `ELF` binary format the name of the dynamic linker is specified as a
string in the special section `.interp`.
```bash
readelf -W --string-dump .interp main

String dump of section '.interp':
  [     0]  /lib64/ld-linux-x86-64.so.2
```

The `.interp` section is referenced by the `PT_INTERP` segment in the program
headers. This segment is used by the Linux Kernel during the `execve(2)`
syscall in the [`load_elf_binary`][load_elf_binary] function to check if the
program needs a dynamic linker and if so to retrieve its name.
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

With the use of `gdb` it can be easily verified that the control is first
passed to the dynamic linker and not the user program. This is shown by
stopping at the first instruction of the new process (`starti`) and examining
the backtrace (`bt`). Where `ld-linux-x86-64.so` is the dynamic linker as shown
in the `.interp` section above.
```bash
gdb -q --batch -ex 'starti' -ex 'bt' ./main

Program stopped.
0x00007ffff7fd2090 in _start () from /lib64/ld-linux-x86-64.so.2
#0  0x00007ffff7fd2090 in _start () from /lib64/ld-linux-x86-64.so.2
#1  0x0000000000000001 in ?? ()
#2  0x00007fffffffe43e in ?? ()
#3  0x0000000000000000 in ?? ()
```
> NOTE: Frames `#1 - #3` don't actually exist, gdb's unwinder just tried to further unwind the stack.

## Things to remember
- Dynamically linked programs use code contained in separate library files.
- The `dynamic linker` is an interpreter loaded by the Linux Kernel and gets
  control before the user program.
- A dynamically linked program specifies the dynamic linker needed in the
  `.interp` section.

[load_elf_binary]: https://elixir.bootlin.com/linux/v5.9.8/source/fs/binfmt_elf.c#L850
