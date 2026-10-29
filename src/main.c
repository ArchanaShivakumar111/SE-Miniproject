
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "alias.h"
#include "history.h"
#include "jobs.h"
#include "shell.h"

static void print_prompt(void)
{
    char host[256] = "localhost";
    char cwd[PATH_MAX] = "?";
    const struct passwd *pw = getpwuid(getuid());

    gethostname(host, sizeof(host) - 1);

    if (!getcwd(cwd, sizeof(cwd)))
        cwd[0] = '?';

    printf("%s@%s:%s$ ",
           pw ? pw->pw_name : "user", host, cwd);
    fflush(stdout);
}

static int initialize_terminal(Shell *sh)
{
    sh->interactive = isatty(STDIN_FILENO);
    sh->shell_pgid = getpgrp();

    if (!sh->interactive)
        return 0;

    /* Wait until this shell's process group owns the terminal. */
    while (tcgetpgrp(STDIN_FILENO) != getpgrp()) {
        if (kill(-getpgrp(), SIGTTIN) < 0)
            return -1;
    }

    /* Become a process-group leader when possible. */
    if (setpgid(0, 0) < 0 &&
        errno != EACCES && errno != EPERM)
        return -1;

    sh->shell_pgid = getpgrp();

    if (tcsetpgrp(STDIN_FILENO, sh->shell_pgid) < 0)
        return -1;

    /* The shell must survive terminal-generated job-control signals. */
    if (signal(SIGINT, SIG_IGN) == SIG_ERR ||
        signal(SIGTSTP, SIG_IGN) == SIG_ERR ||
        signal(SIGQUIT, SIG_IGN) == SIG_ERR ||
        signal(SIGTTIN, SIG_IGN) == SIG_ERR ||
        signal(SIGTTOU, SIG_IGN) == SIG_ERR)
        return -1;

    return 0;
}

int main(void)
{
    Shell sh = {0};
    sh.running = 1;

    if (initialize_terminal(&sh) < 0) {
        perror("simple_shell: terminal initialization");
        return 1;
    }

    sh.history = history_create();
    sh.aliases = alias_create();
    sh.jobs = job_table_create();

    char *hist_path = history_default_path();

    if (sh.history && hist_path)
        history_load(sh.history, hist_path);

    char *line = NULL;
    size_t cap = 0;

    while (sh.running) {
        job_table_reap(sh.jobs);

        if (sh.interactive)
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
    job_table_reap(sh.jobs);
    job_table_destroy(sh.jobs);
    alias_destroy(sh.aliases);

    free(hist_path);
    free(line);

    return sh.last_status;
}
