#include "shell.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "alias.h"
#include "builtins.h"
#include "lexer.h"
#include "redirection.h"

static int run_external(char **argv)
{
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "simple_shell: fork: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "simple_shell: %s: %s\n",
                argv[0], strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }

    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return 1;
}

int shell_execute_line(Shell *sh, const char *line)
{
    char **argv = NULL;
    int argc = lexer_tokenize(line, &argv);

    if (argc < 0) {
        fprintf(stderr, "simple_shell: syntax error\n");
        sh->last_status = 2;
        return sh->last_status;
    }

    if (argc > 0 && sh->aliases &&
        alias_expand(sh->aliases, &argv, &argc) < 0) {
        fprintf(stderr, "simple_shell: alias expansion failed\n");
        sh->last_status = 1;
        argc = 0;
    }

    if (argc > 0) {
        char **command_argv = NULL;
        int command_argc = 0;
        RedirectionList redirects = {0};
        int saved_fds[3] = {-1, -1, -1};

        if (redirection_extract(argv, argc,
                                &command_argv, &command_argc,
                                &redirects) < 0) {
            fprintf(stderr, "simple_shell: syntax error\n");
            sh->last_status = 2;
        } else if (redirects.count > 0 && command_argc == 0) {
            fprintf(stderr, "simple_shell: syntax error\n");
            sh->last_status = 2;
        } else if (redirection_apply(&redirects, saved_fds) < 0) {
            sh->last_status = 1;
        } else {
            if (command_argc > 0) {
                if (builtin_is(command_argv[0]))
                    sh->last_status =
                        builtin_run(sh, command_argc, command_argv);
                else
                    sh->last_status = run_external(command_argv);
            }

            redirection_restore(saved_fds);
        }

        free(command_argv);
        redirection_free(&redirects);
    }

    lexer_free(argv);
    return sh->last_status;
}
