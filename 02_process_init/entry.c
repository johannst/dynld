// Copyright (c) 2020 Johannes Stoelp

#include <auxv.h>
#include <elf.h>
#include <io.h>

#if !defined(__linux__) || !defined(__x86_64__)
#    error "Only supported in linux(x86_64)!"
#endif

void entry(const uint64_t* prctx) {
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

    // Print the data provided by the Linux Kernel on the stack.

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
}
