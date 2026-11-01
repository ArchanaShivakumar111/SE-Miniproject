
#include "shell.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "alias.h"
#include "builtins.h"
#include "expander.h"
#include "jobs.h"
#include "lexer.h"
#include "pipeline.h"
#include "redirection.h"

static int wait_for_child(pid_t pid)
{
    int status;
    pid_t result;

    do {
        result = waitpid(pid, &status, 0);
    } while (result < 0 && errno == EINTR);

    if (result < 0)
        return 1;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return 1;
}

static void reset_child_signals(void)
{
    (void)signal(SIGINT, SIG_DFL);
    (void)signal(SIGTSTP, SIG_DFL);
    (void)signal(SIGQUIT, SIG_DFL);
    (void)signal(SIGTTIN, SIG_DFL);
    (void)signal(SIGTTOU, SIG_DFL);
}

static int set_parent_process_group(pid_t pid)
{
    if (setpgid(pid, pid) == 0)
        return 0;

    if (errno == EACCES || errno == ESRCH)
        return 0;

    return -1;
}

static void restore_shell_terminal(Shell *sh)
{
    if (sh->interactive &&
        tcsetpgrp(STDIN_FILENO, sh->shell_pgid) < 0) {
        perror("simple_shell: restore terminal");
    }
}

static int wait_for_foreground(
    Shell *sh, pid_t pid, const char *line)
{
    int status;
    pid_t result;

    if (sh->interactive &&
        tcsetpgrp(STDIN_FILENO, pid) < 0) {
        perror("simple_shell: foreground terminal");
        (void)kill(-pid, SIGTERM);
        (void)wait_for_child(pid);
        restore_shell_terminal(sh);
        return 1;
    }

    do {
        result = waitpid(pid, &status, WUNTRACED);
    } while (result < 0 && errno == EINTR);

    restore_shell_terminal(sh);

    if (result < 0) {
        perror("simple_shell: waitpid");
        return 1;
    }

    if (WIFSTOPPED(status)) {
        int job_id;

        if (!sh->jobs ||
            job_table_add_stopped(
                sh->jobs, pid, line, &job_id) < 0) {
            fprintf(stderr,
                    "simple_shell: cannot track stopped job\n");

            (void)kill(-pid, SIGKILL);

            while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
                ;

            return 1;
        }

        printf("[%d] Stopped %s", job_id, line);

        size_t len = strlen(line);
        if (len == 0 || line[len - 1] != '\n')
            putchar('\n');

        fflush(stdout);
        return 128 + WSTOPSIG(status);
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return 1;
}

static int register_background_job(
    Shell *sh, pid_t pid, const char *line, int status_fd)
{
    int job_id;

    if (!sh->jobs ||
        job_table_add(sh->jobs, pid, line, &job_id) < 0) {
        fprintf(stderr,
                "simple_shell: cannot track background job\n");

        (void)kill(-pid, SIGTERM);
        (void)wait_for_child(pid);
        return 1;
    }

    int output_fd = status_fd >= 0
                        ? status_fd
                        : STDOUT_FILENO;

    if (dprintf(output_fd, "[%d] %ld\n",
                job_id, (long)pid) < 0) {
        fprintf(stderr,
                "simple_shell: cannot print background job\n");
    }

    return 0;
}

static int run_external(
    Shell *sh, char **argv, int background,
    const char *line, int status_fd)
{
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "simple_shell: fork: %s\n",
                strerror(errno));
        return 1;
    }

    if (pid == 0) {
        if (setpgid(0, 0) < 0) {
            fprintf(stderr, "simple_shell: setpgid: %s\n",
                    strerror(errno));
            _exit(126);
        }

        reset_child_signals();

        execvp(argv[0], argv);

        int exec_error = errno;
        fprintf(stderr, "simple_shell: %s: %s\n",
                argv[0], strerror(exec_error));

        _exit(exec_error == ENOENT ? 127 : 126);
    }

    if (set_parent_process_group(pid) < 0) {
        fprintf(stderr, "simple_shell: setpgid: %s\n",
                strerror(errno));

        (void)kill(pid, SIGTERM);
        (void)wait_for_child(pid);
        return 1;
    }

    if (background)
        return register_background_job(sh, pid, line, status_fd);

    return wait_for_foreground(sh, pid, line);
}

static int run_background_builtin(
    Shell *sh, int argc, char **argv,
    const char *line, int status_fd)
{
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "simple_shell: fork: %s\n",
                strerror(errno));
        return 1;
    }

    if (pid == 0) {
        if (setpgid(0, 0) < 0) {
            fprintf(stderr, "simple_shell: setpgid: %s\n",
                    strerror(errno));
            _exit(126);
        }

        reset_child_signals();

        int result = builtin_run(sh, argc, argv);

        fflush(stdout);
        fflush(stderr);

        _exit(result < 0 ? 1 : result & 0xff);
    }

    if (set_parent_process_group(pid) < 0) {
        fprintf(stderr, "simple_shell: setpgid: %s\n",
                strerror(errno));

        (void)kill(pid, SIGTERM);
        (void)wait_for_child(pid);
        return 1;
    }

    return register_background_job(sh, pid, line, status_fd);
}

/* Runs a pipeline of 2 or more stages as one job: a child of the shell
 * (its process-group leader) starts every stage and waits for them, so
 * Ctrl+C, Ctrl+Z, fg, bg and the job table treat the pipeline like any
 * other single process. */
static int run_pipeline_job(
    Shell *sh, const Pipeline *pl, int background, const char *line)
{
    /* Unflushed output would otherwise be written once per child. */
    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "simple_shell: fork: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        if (setpgid(0, 0) < 0) {
            fprintf(stderr, "simple_shell: setpgid: %s\n",
                    strerror(errno));
            _exit(126);
        }

        reset_child_signals();

        int status = pipeline_run_stages(sh, pl);

        fflush(stdout);
        fflush(stderr);
        _exit(status & 0xff);
    }

    if (set_parent_process_group(pid) < 0) {
        fprintf(stderr, "simple_shell: setpgid: %s\n", strerror(errno));

        (void)kill(pid, SIGTERM);
        (void)wait_for_child(pid);
        return 1;
    }

    if (background)
        return register_background_job(sh, pid, line, STDOUT_FILENO);

    return wait_for_foreground(sh, pid, line);
}

/* One command (no pipe): redirections and built-ins act on the shell
 * itself. Returns the command's exit status. */
static int run_simple(
    Shell *sh, char **argv, int argc, int background, const char *line)
{
    char **command_argv = NULL;
    int command_argc = 0;
    RedirectionList redirects = {0};
    int saved_fds[3] = {-1, -1, -1};
    int status;

    if (redirection_extract(
            argv, argc, &command_argv,
            &command_argc, &redirects) < 0) {
        fprintf(stderr, "simple_shell: syntax error\n");
        status = 2;
    } else if (command_argc == 0) {
        fprintf(stderr, "simple_shell: syntax error\n");
        status = 2;
    } else if (redirection_apply(&redirects, saved_fds) < 0) {
        status = 1;
    } else {
        int status_fd =
            saved_fds[STDOUT_FILENO] >= 0
                ? saved_fds[STDOUT_FILENO]
                : STDOUT_FILENO;

        if (builtin_is(command_argv[0])) {
            if (background &&
                builtin_is_job_control(command_argv[0])) {
                fprintf(stderr,
                        "simple_shell: %s cannot run in the background\n",
                        command_argv[0]);
                status = 1;
            } else if (background) {
                status = run_background_builtin(
                    sh, command_argc, command_argv,
                    line, status_fd);
            } else {
                status = builtin_run(
                    sh, command_argc, command_argv);
            }
        } else {
            status = run_external(
                sh, command_argv, background, line, status_fd);
        }

        redirection_restore(saved_fds);
    }

    free(command_argv);
    redirection_free(&redirects);
    return status;
}

/* pipeline_split() with its failures reported to the user. */
static int split_or_report(
    Shell *sh, char **argv, int argc, Pipeline *pl)
{
    int rc = pipeline_split(argv, argc, pl);

    if (rc == PIPELINE_TOO_MANY) {
        sh_error(NULL, "too many pipeline stages (max %d)",
                 MAX_PIPELINE_STAGES);
        sh->last_status = ST_SYNTAX;
        return -1;
    }
    if (rc < 0) {
        sh_error(NULL, "out of memory");
        sh->last_status = ST_GENERAL;
        return -1;
    }
    return 0;
}

static int stages_contain_pipe(const Pipeline *pl)
{
    for (int i = 0; i < pl->count; i++)
        for (int j = 0; j < pl->stages[i].argc; j++)
            if (strcmp(pl->stages[i].argv[j], PIPE_MARKER) == 0)
                return 1;
    return 0;
}

/* Alias expansion applies to the first word of every stage; an alias
 * whose value contains '|' adds stages, so split again in that case. */
static int expand_stage_aliases(Shell *sh, Pipeline *pl)
{
    for (int i = 0; i < pl->count; i++) {
        PipeStage *st = &pl->stages[i];

        if (st->argc > 0 &&
            alias_expand(sh->aliases, &st->argv, &st->argc) < 0) {
            fprintf(stderr, "simple_shell: alias expansion failed\n");
            sh->last_status = 1;
            return -1;
        }
    }

    if (stages_contain_pipe(pl)) {
        char **flat = NULL;
        int flat_argc = 0;

        if (pipeline_join(pl, &flat, &flat_argc) < 0) {
            sh_error(NULL, "out of memory");
            sh->last_status = ST_GENERAL;
            return -1;
        }
        return split_or_report(sh, flat, flat_argc, pl);
    }
    return 0;
}

/* A trailing & runs the whole line in the background; anywhere else it
 * is a syntax error. */
static int strip_background(Shell *sh, Pipeline *pl, int *background)
{
    *background = 0;

    for (int s = 0; s < pl->count; s++) {
        PipeStage *st = &pl->stages[s];

        for (int i = 0; i < st->argc; i++) {
            if (strcmp(st->argv[i], "\x1e&") != 0)
                continue;

            if (s != pl->count - 1 || i != st->argc - 1) {
                sh_error(NULL, "syntax error");
                sh->last_status = ST_SYNTAX;
                return -1;
            }

            free(st->argv[i]);
            st->argv[i] = NULL;
            st->argc--;
            *background = 1;
        }
    }
    return 0;
}

/* Turns the lexer's argv into a Pipeline. Takes ownership of argv. On
 * failure everything is freed and the status is already set. */
static int build_pipeline(
    Shell *sh, char **argv, int argc, Pipeline *pl, int *background)
{
    if (split_or_report(sh, argv, argc, pl) < 0)
        return -1;

    if ((sh->aliases && expand_stage_aliases(sh, pl) < 0) ||
        strip_background(sh, pl, background) < 0) {
        pipeline_free(pl);
        return -1;
    }
    return 0;
}

int shell_execute_line(Shell *sh, const char *line)
{
    char **argv = NULL;
    ShError err;
    TokenList *tokens = lex(line, &err);

    if (!tokens) {
        if (err.code == ERR_SYNTAX) {
            sh_error("syntax error", "%s", err.msg);
            sh->last_status = ST_SYNTAX;
        } else {
            sh_error(NULL, "%s", err.msg);
            sh->last_status = ST_GENERAL;
        }
        return sh->last_status;
    }

    /* FR-003: $NAME and $? on tokens, so values are never re-lexed. */
    if (expand(tokens, sh) != 0) {
        token_list_free(tokens);
        sh_error(NULL, "out of memory");
        sh->last_status = ST_GENERAL;
        return sh->last_status;
    }

    int argc = token_list_to_argv(tokens, &argv);
    token_list_free(tokens);

    if (argc == LEXER_EMARKER) {
        sh_error("syntax error", "invalid control character");
        sh->last_status = ST_SYNTAX;
        return sh->last_status;
    }
    if (argc < 0) {
        sh_error(NULL, "out of memory");
        sh->last_status = ST_GENERAL;
        return sh->last_status;
    }

    Pipeline pl;
    int background = 0;

    if (build_pipeline(sh, argv, argc, &pl, &background) < 0)
        return sh->last_status;

    if (pl.count == 1) {
        if (pl.stages[0].argc > 0) {
            sh->last_status = run_simple(
                sh, pl.stages[0].argv, pl.stages[0].argc,
                background, line);
        } else if (background) {        /* a lone & */
            sh_error(NULL, "syntax error");
            sh->last_status = ST_SYNTAX;
        }
    } else if (pipeline_validate(&pl) < 0) {
        sh->last_status = ST_SYNTAX;
    } else {
        sh->last_status = run_pipeline_job(sh, &pl, background, line);
    }

    pipeline_free(&pl);
    return sh->last_status;
}
