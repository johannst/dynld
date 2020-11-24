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

    dynld_printf("Got %d arg(s)\n", argc);
    for (const char** arg = argv; *arg; ++arg) {
        dynld_printf("\targ = %s\n", *arg);
    }

    const int max_env = 10;
    dynld_printf("Print first %d env var(s)\n", max_env - 1);
    for (const char** env = envv; *env && (env - envv < max_env); ++env) {
        dynld_printf("\tenv = %s\n", *env);
    }

    dynld_printf("Print auxiliary vector\n");
    dynld_printf("\tAT_EXECFD: %ld\n", auxv[AT_EXECFD]);
    dynld_printf("\tAT_PHDR  : %p\n", auxv[AT_PHDR]);
    dynld_printf("\tAT_PHENT : %ld\n", auxv[AT_PHENT]);
    dynld_printf("\tAT_PHNUM : %ld\n", auxv[AT_PHNUM]);
    dynld_printf("\tAT_PAGESZ: %ld\n", auxv[AT_PAGESZ]);
    dynld_printf("\tAT_BASE  : %lx\n", auxv[AT_BASE]);
    dynld_printf("\tAT_FLAGS : %ld\n", auxv[AT_FLAGS]);
    dynld_printf("\tAT_ENTRY : %p\n", auxv[AT_ENTRY]);
    dynld_printf("\tAT_NOTELF: %lx\n", auxv[AT_NOTELF]);
    dynld_printf("\tAT_UID   : %ld\n", auxv[AT_UID]);
    dynld_printf("\tAT_EUID  : %ld\n", auxv[AT_EUID]);
    dynld_printf("\tAT_GID   : %ld\n", auxv[AT_GID]);
    dynld_printf("\tAT_EGID  : %ld\n", auxv[AT_EGID]);
}
