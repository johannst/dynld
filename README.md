# dynld

This repository contains studies about `process initialization` and `dynamic
linking` on Linux(x86_64). It is mainly used for personal studies, but may be
useful to some random internet user.

All studies and discussions assume the following environment:

- Arch: `x86_64`
- OS: `Linux Kernel`
- System ABI: `SystemV x86_64 ABI`
- BinFMT: `ELF`

The studies are structured as follows:

1. [Ch 01 dynamic linking](./01_dynamic_linking):
   Brief introduction to dynamic linking.
1. [Ch 02 process initialization](./02_process_init):
   Building a `no-std` executable and exploring the initial process state.
1. [Ch 03 dynamic linker skeleton](./03_hello_dynld):
   Building a skeleton for the dynamic linker which can run a `statically
   linked no-std` executable.
1. [Ch 04 dynld no-std](./04_dynld_nostd):
   Building a dynamic linker `dynld` which can initialize the execution
   environment for a `no-std` executable with a shared library dependency.

## License
This project is licensed under the [MIT](LICENSE) license.
