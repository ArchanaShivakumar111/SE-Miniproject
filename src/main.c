#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "history.h"
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
    sh.history = history_create();
    char *hist_path = history_default_path();
    if (sh.history && hist_path)
        history_load(sh.history, hist_path);
    char *line = NULL;
    size_t cap = 0;
    int tty = isatty(STDIN_FILENO);

    while (sh.running) {
        if (tty)
            print_prompt();
        if (getline(&line, &cap, stdin) < 0)
            break;
        if (sh.history)
            history_add(sh.history, line);
        shell_execute_line(&sh, line);
    }
    if (sh.history && hist_path)
        history_save(sh.history, hist_path);
    history_destroy(sh.history);
    free(hist_path);
    free(line);
    return sh.last_status;
}
