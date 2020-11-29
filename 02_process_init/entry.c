// Copyright (c) 2020 Johannes Stoelp

#include <elf.h>
#define MAX_PRINTF_LEN 128
#include <io.h>

#if !defined(__linux__) || !defined(__x86_64__)
#    error "Only supported in linux(x86_64)!"
#endif

void entry(const long* prctx) {
    // Interpret data on the stack passed by the OS kernel as specified in the
    // x86_64 SysV ABI.

    long argc = *prctx;
    const char** argv = (const char**)(prctx + 1);
    const char** envv = (const char**)(argv + argc + 1);

    int envc = 0;
    for (const char** env = envv; *env; ++env) {
        ++envc;
    }

    uint64_t auxv[AT_MAX_CNT];
    for (unsigned i = 0; i < AT_MAX_CNT; ++i) {
        auxv[i] = 0;
    }

    const uint64_t* auxvp = (const uint64_t*)(envv + envc + 1);
    for (unsigned i = 0; auxvp[i] != AT_NULL; i += 2) {
        if (auxvp[i] < AT_MAX_CNT) {
            auxv[auxvp[i]] = auxvp[i + 1];
        }
    }

    // Print for demonstration

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
