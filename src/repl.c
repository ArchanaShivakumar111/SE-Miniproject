#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "executor.h"
#include "repl.h"

static void print_prompt(const ShellContext *ctx)
{
    if (ctx->interactive) {
        printf("%s@%s:%s$ ", ctx->username, ctx->hostname, ctx->cwd);
        fflush(stdout);
    }
}

int repl_run(ShellContext *ctx)
{
    char line[MAX_LINE];

    while (ctx->running) {
        print_prompt(ctx);
        if (fgets(line, sizeof line, stdin) == NULL)
            break;                      /* EOF (Ctrl+D) */
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0')
            continue;
        executor_run_line(ctx, line);
    }
    if (ctx->interactive)
        putchar('\n');
    return ctx->exit_code;
}
