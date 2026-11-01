#ifndef ENV_H
#define ENV_H

#include "shell.h"

/* SAD C-10 environment access. Names must match [A-Za-z_][A-Za-z0-9_]*. */
int env_name_valid(const char *name);
const char *env_get(const char *name);
/* 0 on success, 1 on an invalid name or setenv/unsetenv failure. */
int env_set(const char *name, const char *value);
int env_unset(const char *name);

/* FR-009b built-ins (SAD 10.3), registered in builtins.c. */
int builtin_export(Shell *sh, int argc, char **argv);
int builtin_unset(Shell *sh, int argc, char **argv);
int builtin_env(Shell *sh, int argc, char **argv);

#endif
