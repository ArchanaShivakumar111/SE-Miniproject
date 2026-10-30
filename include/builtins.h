#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdbool.h>
#include "shell.h"

/* FR-009a/b/c: cd, pwd, echo, exit, export, unset, env, jobs, fg, bg */
bool builtin_is_builtin(const char *name);
int  builtin_run(ShellContext *ctx, char *const argv[]);

#endif /* BUILTINS_H */
