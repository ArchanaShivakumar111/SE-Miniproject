#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "executor.h"

#define MAX_ARGS 256

/*
 * Temporary splitter: breaks a line on spaces/tabs (modifies `line`).
 * TODO: replace with the FR-002 lexer (quotes, escapes, operators) once
 * it is merged.  Returns argc, or -1 on error.
 */
static int split_args(char *line, char *argv[], int max)
{
    int   argc = 0;
    char *save = NULL;
    char *tok  = strtok_r(line, " \t", &save);

    while (tok != NULL) {
        if (argc >= max - 1) {
            shell_error("too many arguments");
            return -1;
        }
        argv[argc++] = tok;
        tok = strtok_r(NULL, " \t", &save);
    }
    argv[argc] = NULL;
    return argc;
}

/* Runs in the child process only. Never returns. */
static void child_exec(char *const argv[])
{
    int err;

    /* Children must react to Ctrl+C / Ctrl+Z normally (FR-008). */
    signal(SIGINT,  SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

    execvp(argv[0], argv);          /* searches PATH (FR-004) */

    err = errno;
    if (err == ENOENT)
        shell_error("%s: command not found", argv[0]);
    else
        shell_error("%s: %s", argv[0], strerror(err));
    _exit(err == ENOENT ? 127 : 126);
}

/* Waits for pid and converts its status to a shell exit status ($?). */
static int wait_for(pid_t pid)
{
    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            shell_error("waitpid: %s", strerror(errno));
            return 1;
        }
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}

/* Runs one simple command in the foreground and records its status. */
static int run_command(ShellContext *ctx, char *const argv[])
{
    pid_t pid = fork();

    if (pid < 0) {
        shell_error("fork: %s", strerror(errno));
        ctx->last_status = 1;
        return -1;
    }
    if (pid == 0)
        child_exec(argv);

    ctx->last_status = wait_for(pid);
    return 0;
}

int executor_run_line(ShellContext *ctx, const char *line)
{
    char  buf[MAX_LINE];
    char *argv[MAX_ARGS];
    int   argc;

    snprintf(buf, sizeof buf, "%s", line);
    argc = split_args(buf, argv, MAX_ARGS);
    if (argc < 0) {
        ctx->last_status = 1;
        return -1;
    }
    if (argc == 0)
        return 0;

    /* TODO FR-009a/b/c: dispatch built-ins here before forking.
     * TODO FR-005: pipelines.   TODO FR-006: redirection. */
    return run_command(ctx, argv);
}
