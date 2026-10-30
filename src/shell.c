#define _POSIX_C_SOURCE 200809L

#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "shell.h"

void shell_error(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s: ", SHELL_NAME);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

void ctx_refresh_cwd(ShellContext *ctx)
{
    if (getcwd(ctx->cwd, sizeof ctx->cwd) == NULL) {
        shell_error("getcwd failed");
        ctx->cwd[0] = '?';
        ctx->cwd[1] = '\0';
    }
}

int ctx_init(ShellContext *ctx)
{
    struct passwd *pw;

    memset(ctx, 0, sizeof *ctx);
    ctx->running     = true;
    ctx->interactive = isatty(STDIN_FILENO) != 0;
    ctx->shell_pgid  = getpgrp();

    pw = getpwuid(getuid());
    snprintf(ctx->username, sizeof ctx->username, "%s",
             pw != NULL ? pw->pw_name : "user");

    if (gethostname(ctx->hostname, sizeof ctx->hostname) != 0)
        snprintf(ctx->hostname, sizeof ctx->hostname, "localhost");
    ctx->hostname[sizeof ctx->hostname - 1] = '\0';

    ctx_refresh_cwd(ctx);
    return 0;
}

void ctx_destroy(ShellContext *ctx)
{
    /* Other modules free ctx->history / ctx->jobs via their own destroy(). */
    (void)ctx;
}
