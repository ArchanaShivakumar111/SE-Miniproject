#define _POSIX_C_SOURCE 200809L

#include "executor.h"

int executor_run_line(ShellContext *ctx, const char *line)
{
    /* TODO FR-004: fork/execvp via PATH
     * TODO FR-005: pipelines     TODO FR-006: redirection */
    (void)line;
    ctx->last_status = 0;
    return 0;
}
