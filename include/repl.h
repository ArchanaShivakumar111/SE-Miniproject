#ifndef REPL_H
#define REPL_H

#include "shell.h"

/* Builds "user@host:cwd$ " with home shown as ~ (FR-001). Returns a
 * malloc'd string, or a malloc'd "$ " if user, host or cwd is NULL or
 * empty. Returns NULL only on allocation failure. home may be NULL. */
char *prompt_format(const char *user, const char *host,
                    const char *cwd, const char *home);

/* Writes the prompt for the current user, host and directory to stdout and
 * flushes. Falls back to "$ " if any lookup fails. */
void prompt_render(const Shell *sh);

/* 1 if line holds only blanks (BR-5). */
int line_is_blank(const char *line);

#endif
