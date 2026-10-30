#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "shell.h"

/*
 * FR-004 / FR-005 / FR-006: run one input line (commands, pipelines,
 * redirection). Updates ctx->last_status. Returns 0 on success, -1 on
 * a shell-level error.
 */
int executor_run_line(ShellContext *ctx, const char *line);

#endif /* EXECUTOR_H */
