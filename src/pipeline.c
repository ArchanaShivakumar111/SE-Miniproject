#include "pipeline.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "error.h"
#include "redirection.h"

static int is_pipe(const char *word)
{
    return strcmp(word, PIPE_MARKER) == 0;
}

/* ------------------------------------------------------------------ */
/* Splitting and joining                                               */
/* ------------------------------------------------------------------ */

void pipeline_free(Pipeline *pl)
{
    if (!pl)
        return;

    for (int i = 0; i < pl->count; i++) {
        for (int j = 0; j < pl->stages[i].argc; j++)
            free(pl->stages[i].argv[j]);
        free(pl->stages[i].argv);
        pl->stages[i].argv = NULL;
        pl->stages[i].argc = 0;
    }
    pl->count = 0;
}

/* Frees the strings still owned by argv (moved ones are NULLed). */
static void drop_argv(char **argv, int argc)
{
    for (int i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
}

int pipeline_split(char **argv, int argc, Pipeline *pl)
{
    int start = 0;

    memset(pl, 0, sizeof *pl);

    for (int i = 0; i <= argc; i++) {
        if (i < argc && !is_pipe(argv[i]))
            continue;

        if (pl->count == MAX_PIPELINE_STAGES) {
            pipeline_free(pl);
            drop_argv(argv, argc);
            return PIPELINE_TOO_MANY;
        }

        int len = i - start;
        char **v = calloc((size_t)len + 1, sizeof *v);

        if (!v) {
            pipeline_free(pl);
            drop_argv(argv, argc);
            return PIPELINE_NOMEM;
        }
        for (int j = 0; j < len; j++) {
            v[j] = argv[start + j];
            argv[start + j] = NULL;
        }

        pl->stages[pl->count].argv = v;
        pl->stages[pl->count].argc = len;
        pl->count++;

        if (i < argc) {             /* the marker itself is not needed */
            free(argv[i]);
            argv[i] = NULL;
        }
        start = i + 1;
    }

    free(argv);
    return 0;
}

int pipeline_join(Pipeline *pl, char ***argv, int *argc)
{
    int total = pl->count > 0 ? pl->count - 1 : 0;

    for (int i = 0; i < pl->count; i++)
        total += pl->stages[i].argc;

    char **v = calloc((size_t)total + 1, sizeof *v);

    if (!v)
        return -1;

    /* First place every separator (the only step that can fail), so that
     * on failure pl is untouched. */
    int pos = 0;

    for (int i = 0; i < pl->count; i++) {
        pos += pl->stages[i].argc;
        if (i < pl->count - 1) {
            v[pos] = strdup(PIPE_MARKER);
            if (!v[pos]) {
                for (int k = 0; k < pos; k++)
                    free(v[k]);         /* only separators are set so far */
                free(v);
                return -1;
            }
            pos++;
        }
    }

    pos = 0;
    for (int i = 0; i < pl->count; i++) {
        for (int j = 0; j < pl->stages[i].argc; j++)
            v[pos++] = pl->stages[i].argv[j];
        pos++;                          /* skip the separator slot */
        free(pl->stages[i].argv);
        pl->stages[i].argv = NULL;
        pl->stages[i].argc = 0;
    }

    *argv = v;
    *argc = total;
    pl->count = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Validation (runs in the shell, before anything is forked)           */
/* ------------------------------------------------------------------ */

int pipeline_validate(const Pipeline *pl)
{
    for (int i = 0; i < pl->count; i++) {
        const PipeStage *st = &pl->stages[i];
        char **cargv = NULL;
        int cargc = 0;
        RedirectionList rl = {0};

        if (st->argc == 0) {
            sh_error("syntax error", "near unexpected token '|'");
            return -1;
        }
        if (redirection_extract(st->argv, st->argc,
                                &cargv, &cargc, &rl) < 0 || cargc == 0) {
            free(cargv);
            redirection_free(&rl);
            sh_error(NULL, "syntax error");
            return -1;
        }
        free(cargv);
        redirection_free(&rl);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Running the stages                                                  */
/* ------------------------------------------------------------------ */

static void close_fd(int fd)
{
    if (fd >= 0)
        close(fd);
}

/* Both ends are close-on-exec (SEC-007); dup2() clears the flag on the
 * copy that becomes stdin or stdout, so only those survive an exec. */
static int make_pipe(int fd[2])
{
    if (pipe(fd) < 0)
        return -1;

    for (int i = 0; i < 2; i++) {
        if (fcntl(fd[i], F_SETFD, FD_CLOEXEC) < 0) {
            int saved = errno;

            close(fd[0]);
            close(fd[1]);
            errno = saved;
            return -1;
        }
    }
    return 0;
}

/* Child side: make `from` the process's fd `to`, then drop the original. */
static int move_fd(int from, int to)
{
    if (from < 0)
        return 0;

    if (from == to) {
        int flags = fcntl(to, F_GETFD);

        return (flags >= 0 &&
                fcntl(to, F_SETFD, flags & ~FD_CLOEXEC) == 0) ? 0 : -1;
    }
    if (dup2(from, to) < 0) {
        sh_error(NULL, "dup2: %s", strerror(errno));
        return -1;
    }
    close(from);
    return 0;
}

/* Runs one stage in a freshly forked child. Never returns. */
static void stage_main(Shell *sh, const PipeStage *st,
                       int in_fd, int out_fd, int spare_fd)
{
    char **cargv = NULL;
    int cargc = 0;
    RedirectionList rl = {0};
    int saved_fds[3];

    close_fd(spare_fd);

    (void)signal(SIGPIPE, SIG_DFL);

    /* Pipe first, then redirections, so "cmd > file | next" sends the
     * output to the file, as other shells do. */
    if (move_fd(in_fd, STDIN_FILENO) < 0 ||
        move_fd(out_fd, STDOUT_FILENO) < 0)
        _exit(ST_GENERAL);

    if (redirection_extract(st->argv, st->argc, &cargv, &cargc, &rl) < 0 ||
        cargc == 0) {
        sh_error(NULL, "syntax error");
        _exit(ST_SYNTAX);
    }
    if (redirection_apply(&rl, saved_fds) < 0)
        _exit(ST_GENERAL);          /* the message was already printed */

    if (builtin_is(cargv[0])) {
        /* SEC-005: this is a copy of the shell; whatever the built-in
         * changes is lost when this child exits. */
        int result = builtin_run(sh, cargc, cargv);

        fflush(stdout);
        fflush(stderr);
        _exit(result < 0 ? ST_GENERAL : result & 0xff);
    }

    execvp(cargv[0], cargv);

    int err = errno;

    sh_error(cargv[0], "%s", strerror(err));
    _exit(err == ENOENT ? ST_NOTFOUND : ST_NOEXEC);
}

static int wait_stage(pid_t pid)
{
    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return ST_GENERAL;
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return ST_SIGNAL_BASE + WTERMSIG(status);
    return ST_GENERAL;
}

int pipeline_run_stages(Shell *sh, const Pipeline *pl)
{
    pid_t pids[MAX_PIPELINE_STAGES];
    int started = 0;
    int prev_read = -1;
    int ok = 1;
    int status = ST_GENERAL;

    for (int i = 0; ok && i < pl->count; i++) {
        int fds[2] = {-1, -1};

        if (i < pl->count - 1 && make_pipe(fds) < 0) {
            sh_error(NULL, "pipe: %s", strerror(errno));
            ok = 0;
            break;
        }

        pid_t pid = fork();

        if (pid < 0) {
            sh_error(NULL, "fork: %s", strerror(errno));
            close_fd(fds[0]);
            close_fd(fds[1]);
            ok = 0;
            break;
        }
        if (pid == 0)
            stage_main(sh, &pl->stages[i], prev_read, fds[1], fds[0]);

        pids[started++] = pid;
        close_fd(prev_read);
        close_fd(fds[1]);
        prev_read = fds[0];
    }

    /* Every pipe end must be closed here or readers never see EOF. */
    close_fd(prev_read);

    for (int i = 0; i < started; i++)   /* reap every child: no zombies */
        status = wait_stage(pids[i]);

    return ok ? status : ST_GENERAL;
}
