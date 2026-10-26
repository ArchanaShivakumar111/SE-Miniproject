#include "shell.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "lexer.h"

static int run_external(char **argv)
{
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "simple_shell: fork: %s\n", strerror(errno));
        return 1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "simple_shell: %s: %s\n", argv[0], strerror(errno));
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
    if (argc > 0) {
        if (builtin_is(argv[0]))
            sh->last_status = builtin_run(sh, argc, argv);
        else
            sh->last_status = run_external(argv);
    }
    lexer_free(argv);
    return sh->last_status;
}
