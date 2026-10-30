
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
#include "jobs.h"
#include "lexer.h"
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

    int argc = token_list_to_argv(tokens, &argv);
    token_list_free(tokens);

    if (argc < 0) {
        sh_error(NULL, "out of memory");
        sh->last_status = ST_GENERAL;
        return sh->last_status;
    }

    if (argc > 0 && sh->aliases &&
        alias_expand(sh->aliases, &argv, &argc) < 0) {
        fprintf(stderr,
                "simple_shell: alias expansion failed\n");
        sh->last_status = 1;
        argc = 0;
    }

    int background = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "\x1e&") == 0) {
            if (i != argc - 1) {
                fprintf(stderr,
                        "simple_shell: syntax error\n");
                sh->last_status = 2;
                lexer_free(argv);
                return sh->last_status;
            }

            free(argv[i]);
            argv[i] = NULL;
            argc--;
            background = 1;
            break;
        }
    }

    if (background && argc == 0) {
        fprintf(stderr, "simple_shell: syntax error\n");
        sh->last_status = 2;
    } else if (argc > 0) {
        char **command_argv = NULL;
        int command_argc = 0;
        RedirectionList redirects = {0};
        int saved_fds[3] = {-1, -1, -1};

        if (redirection_extract(
                argv, argc, &command_argv,
                &command_argc, &redirects) < 0) {
            fprintf(stderr, "simple_shell: syntax error\n");
            sh->last_status = 2;
        } else if (command_argc == 0) {
            fprintf(stderr, "simple_shell: syntax error\n");
            sh->last_status = 2;
        } else if (redirection_apply(&redirects, saved_fds) < 0) {
            sh->last_status = 1;
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
                    sh->last_status = 1;
                } else if (background) {
                    sh->last_status =
                        run_background_builtin(
                            sh, command_argc, command_argv,
                            line, status_fd);
                } else {
                    sh->last_status =
                        builtin_run(
                            sh, command_argc, command_argv);
                }
            } else {
                sh->last_status =
                    run_external(
                        sh, command_argv, background,
                        line, status_fd);
            }

            redirection_restore(saved_fds);
        }

        free(command_argv);
        redirection_free(&redirects);
    }

    lexer_free(argv);
    return sh->last_status;
}
