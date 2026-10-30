#define _POSIX_C_SOURCE 200809L

#include "repl.h"
#include "shell.h"

int main(void)
{
    ShellContext ctx;
    int rc;

    if (ctx_init(&ctx) != 0)
        return 1;
    /* TODO FR-012: if argv has a script file, run non-interactively */
    rc = repl_run(&ctx);
    ctx_destroy(&ctx);
    return rc;
}
