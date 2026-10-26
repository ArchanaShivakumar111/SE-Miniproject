#ifndef BUILTINS_H
#define BUILTINS_H

#include "shell.h"

int builtin_is(const char *name);
int builtin_run(Shell *sh, int argc, char **argv);

#endif
