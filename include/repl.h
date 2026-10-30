#ifndef REPL_H
#define REPL_H

#include "shell.h"

/* FR-001: interactive loop, prints user@host:cwd$ prompt. Returns exit code. */
int repl_run(ShellContext *ctx);

#endif /* REPL_H */
