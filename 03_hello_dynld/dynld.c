// Copyright (c) 2021 Johannes Stoelp

#include <syscall.h>
#include <auxv.h>
#include <io.h>

#include <stdint.h>
#include <asm/unistd.h>

#if !defined(__linux__) || !defined(__x86_64__)
#    error "Only supported in linux(x86_64)!"
#endif

void dl_entry(const uint64_t* prctx) {
    // Interpret data on the stack passed by the OS kernel as specified in the
    // x86_64 SysV ABI.
    uint64_t argc = *prctx;
    const char** argv = (const char**)(prctx + 1);
    const char** envv = (const char**)(argv + argc + 1);

    // Count the number of environment variables in the `ENVP` segment.
    int envc = 0;
    for (const char** env = envv; *env; ++env) {
        ++envc;
    }

    uint64_t auxv[AT_MAX_CNT];
    for (unsigned i = 0; i < AT_MAX_CNT; ++i) {
        auxv[i] = 0;
    }

    // Read the `AUXV` auxiliary vector segment.
    const Auxv64Entry* auxvp = (const Auxv64Entry*)(envv + envc + 1);
    for (; auxvp->tag != AT_NULL; ++auxvp) {
        if (auxvp->tag < AT_MAX_CNT) {
            auxv[auxvp->tag] = auxvp->val;
        }
    }

    // Get address of the entrypoint for the user executable and
    // transfer control.
    // Requirements for the user executable:
    //   - no dependencies
    //   - no relocations

    pfmt("[dynld]: Running %s @ %s\n", __FUNCTION__, __FILE__);

    // Either `AT_EXECFD` or `AT_PHDR` must be specified, we only
    // support `AT_PHDR` here.
    //
    // From the X86_64 SystemV ABI:
    // AT_EXECFD
    //   At process creation the system may pass control to an
    //   interpreter program. When this happens, the system places
    //   either an entry of type `AT_EXECFD` or one of type `AT_PHDR`
    //   in the auxiliary vector. The entry for type `AT_EXECFD`
    //   contains a file descriptor open to read the application
    //   program’s object file.
    //
    // AT_PHDR
    //   The system may create the memory image of the application
    //   program before passing control to the interpreter
    //   program. When this happens the `AT_PHDR` entry tells the
    //   interpreter where to find the program header table in the
    //   memory image.
    if (auxv[AT_PHDR] == 0 || auxv[AT_EXECFD] != 0) {
        pfmt("[dynld]: ERROR, expected Linux Kernel to map user executable!\n");
        syscall1(__NR_exit, 1);
    }

    if (auxv[AT_ENTRY] == 0) {
        pfmt("[dynld]: ERROR, AT_ENTRY not found in auxiliary vector!\n");
        syscall1(__NR_exit, 1);
    }

    // Transfer control to user executable.
    void (*user_entry)() = (void (*)())auxv[AT_ENTRY];
    pfmt("[dynld]: Got user entrypoint @0x%x\n", user_entry);
    user_entry();

    syscall1(__NR_exit, 0);
}
