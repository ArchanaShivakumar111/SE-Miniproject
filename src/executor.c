#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "executor.h"

#define MAX_ARGS 256

/* ------------------------------------------------------------------ */
/* Parsing helpers (temporary until the FR-002 lexer is merged)        */
/* ------------------------------------------------------------------ */

/*
 * Splits a line into pipeline stages on '|' (modifies `line`).
 * Returns the number of stages, or -1 if there are more than `max`.
 * TODO: the lexer will make this quote-aware ("a|b" must not split).
 */
static int split_stages(char *line, char *stages[], int max)
{
    int   n = 0;
    char *p = line;

    for (;;) {
        char *bar = strchr(p, '|');

        if (n >= max) {
            shell_error("too many pipeline stages (max %d)", max);
            return -1;
        }
        stages[n++] = p;
        if (bar == NULL)
            break;
        *bar = '\0';
        p = bar + 1;
    }
    return n;
}

/* Splits one stage on spaces/tabs (modifies `stage`). Returns argc or -1. */
static int split_args(char *stage, char *argv[], int max)
{
    int   argc = 0;
    char *save = NULL;
    char *tok  = strtok_r(stage, " \t", &save);

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

/* ------------------------------------------------------------------ */
/* Process helpers                                                     */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Pipelines (FR-005)                                                  */
/* ------------------------------------------------------------------ */

/*
 * Creates a pipe whose ends are close-on-exec (SEC-007). dup2() clears the
 * flag on the copy that becomes stdin/stdout, so only those survive exec().
 */
static int make_pipe(int fd[2])
{
    int i;

    if (pipe(fd) < 0)
        return -1;
    for (i = 0; i < 2; i++) {
        if (fcntl(fd[i], F_SETFD, FD_CLOEXEC) < 0) {
            close(fd[0]);
            close(fd[1]);
            return -1;
        }
    }
    return 0;
}

static void close_pipes(int fds[][2], int count)
{
    int i;

    for (i = 0; i < count; i++) {
        close(fds[i][0]);
        close(fds[i][1]);
    }
}

/* Child side: connect `from` to `to`, or die. */
static void redirect_or_die(int from, int to)
{
    if (dup2(from, to) < 0) {
        shell_error("dup2: %s", strerror(errno));
        _exit(1);
    }
}

/*
 * Runs argvs[0] | argvs[1] | ... | argvs[n-1].  All stages run
 * concurrently; $? becomes the exit status of the LAST stage.
 */
static int run_pipeline(ShellContext *ctx, char *argvs[][MAX_ARGS], int n)
{
    int   fds[MAX_PIPELINE_STAGES - 1][2];
    pid_t pids[MAX_PIPELINE_STAGES];
    int   made = 0, started = 0, ok = 1, status = 1, i;

    while (ok && made < n - 1) {
        if (make_pipe(fds[made]) < 0) {
            shell_error("pipe: %s", strerror(errno));
            ok = 0;
        } else {
            made++;
        }
    }

    while (ok && started < n) {
        pid_t pid = fork();

        if (pid < 0) {
            shell_error("fork: %s", strerror(errno));
            ok = 0;
        } else if (pid == 0) {
            if (started > 0)
                redirect_or_die(fds[started - 1][0], STDIN_FILENO);
            if (started < n - 1)
                redirect_or_die(fds[started][1], STDOUT_FILENO);
            child_exec(argvs[started]);
        } else {
            pids[started++] = pid;
        }
    }

    /* Parent must close every pipe end, or readers never see EOF. */
    close_pipes(fds, made);

    for (i = 0; i < started; i++)
        status = wait_for(pids[i]);     /* reap every child: no zombies */

    ctx->last_status = ok ? status : 1;
    return ok ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int executor_run_line(ShellContext *ctx, const char *line)
{
    char  buf[MAX_LINE];
    char *stages[MAX_PIPELINE_STAGES];
    char *argvs[MAX_PIPELINE_STAGES][MAX_ARGS];
    int   argcs[MAX_PIPELINE_STAGES];
    int   n, i;

    snprintf(buf, sizeof buf, "%s", line);

    n = split_stages(buf, stages, MAX_PIPELINE_STAGES);
    if (n < 0) {
        ctx->last_status = 2;
        return -1;
    }

    for (i = 0; i < n; i++) {
        argcs[i] = split_args(stages[i], argvs[i], MAX_ARGS);
        if (argcs[i] < 0) {
            ctx->last_status = 1;
            return -1;
        }
    }

    if (n == 1) {
        if (argcs[0] == 0)
            return 0;                   /* blank line */
        /* TODO FR-009a/b/c: dispatch built-ins here before forking.
         * TODO FR-006: redirection. */
        return run_command(ctx, argvs[0]);
    }

    for (i = 0; i < n; i++) {
        if (argcs[i] == 0) {
            shell_error("syntax error near unexpected token '|'");
            ctx->last_status = 2;
            return -1;
        }
    }
    return run_pipeline(ctx, argvs, n);
}
