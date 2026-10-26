#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "shell.h"

static void print_prompt(void)
{
    char host[256] = "localhost";
    char cwd[PATH_MAX] = "?";
    struct passwd *pw = getpwuid(getuid());
    gethostname(host, sizeof host - 1);
    if (!getcwd(cwd, sizeof cwd))
        cwd[0] = '?';
    printf("%s@%s:%s$ ", pw ? pw->pw_name : "user", host, cwd);
    fflush(stdout);
}

int main(void)
{
    Shell sh = {0};
    sh.running = 1;
    char *line = NULL;
    size_t cap = 0;
    int tty = isatty(STDIN_FILENO);

    while (sh.running) {
        if (tty)
            print_prompt();
        if (getline(&line, &cap, stdin) < 0)
            break;
        shell_execute_line(&sh, line);
    }
    free(line);
    return sh.last_status;
}
