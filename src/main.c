#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "alias.h"
#include "history.h"
#include "jobs.h"
#include "repl.h"
#include "shell.h"

static volatile sig_atomic_t child_event_pending = 0;

static void handle_child_signal(int signal_number)
{
    (void)signal_number;
    child_event_pending = 1;
}

static int install_child_signal_handler(void)
{
    struct sigaction action = {0};

    action.sa_handler = handle_child_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    return sigaction(SIGCHLD, &action, NULL);
}

static void reap_pending_children(JobTable *jobs)
{
    do {
        child_event_pending = 0;
        job_table_reap(jobs);
    } while (child_event_pending);
}

static int ensure_line_capacity(
    char **line, size_t *capacity, size_t needed)
{
    if (*capacity >= needed)
        return 0;

    size_t new_capacity = *capacity ? *capacity : 128;

    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = needed;
            break;
        }

        new_capacity *= 2;
    }

    char *updated = realloc(*line, new_capacity);

    if (!updated)
        return -1;

    *line = updated;
    *capacity = new_capacity;
    return 0;
}

/*
 * Read one command without losing partially read input when SIGCHLD
 * arrives. SIGCHLD is blocked outside pselect and temporarily unblocked
 * atomically while waiting for stdin.
 *
 * Returns the number of bytes read, or -1 on EOF/error.
 */
static ssize_t read_command_line(
    JobTable *jobs, char **line, size_t *capacity)
{
    sigset_t child_set;
    sigset_t old_mask;
    sigset_t wait_mask;
    size_t length = 0;

    sigemptyset(&child_set);
    sigaddset(&child_set, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &child_set, &old_mask) < 0)
        return -1;

    wait_mask = old_mask;
    sigdelset(&wait_mask, SIGCHLD);

    for (;;) {
        /*
         * Reap children before waiting again if SIGCHLD arrived
         * while input was also available.
         */
        if (child_event_pending)
            reap_pending_children(jobs);

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int ready = pselect(
            STDIN_FILENO + 1,
            &readfds,
            NULL,
            NULL,
            NULL,
            &wait_mask);

        if (ready < 0) {
            if (errno == EINTR) {
                reap_pending_children(jobs);
                continue;
            }

            int saved_errno = errno;
            reap_pending_children(jobs);
            (void)sigprocmask(SIG_SETMASK, &old_mask, NULL);
            errno = saved_errno;
            return -1;
        }

        char ch;
        ssize_t count = read(STDIN_FILENO, &ch, 1);

        if (count < 0 && errno == EINTR) {
            reap_pending_children(jobs);
            continue;
        }

        if (count < 0) {
            int saved_errno = errno;
            reap_pending_children(jobs);
            (void)sigprocmask(SIG_SETMASK, &old_mask, NULL);
            errno = saved_errno;
            return -1;
        }

        if (count == 0) {
            reap_pending_children(jobs);

            if (sigprocmask(SIG_SETMASK, &old_mask, NULL) < 0)
                return -1;

            if (length == 0) {
                errno = 0;
                return -1;
            }

            (*line)[length] = '\0';
            return (ssize_t)length;
        }

        if (ensure_line_capacity(line, capacity, length + 2) < 0) {
            int saved_errno = errno;
            reap_pending_children(jobs);
            (void)sigprocmask(SIG_SETMASK, &old_mask, NULL);
            errno = saved_errno;
            return -1;
        }

        (*line)[length++] = ch;

        if (ch == '\n') {
            (*line)[length] = '\0';
            reap_pending_children(jobs);

            if (sigprocmask(SIG_SETMASK, &old_mask, NULL) < 0)
                return -1;

            return (ssize_t)length;
        }
    }
}

static int initialize_terminal(Shell *sh, int has_script_arg)
{
    /* FR-001: interactive only for a terminal with no script argument. */
    sh->interactive = isatty(STDIN_FILENO) && !has_script_arg;
    sh->shell_pgid = getpgrp();

    if (!sh->interactive)
        return 0;

    /* Wait until this shell's process group owns the terminal. */
    while (tcgetpgrp(STDIN_FILENO) != getpgrp()) {
        if (kill(-getpgrp(), SIGTTIN) < 0)
            return -1;
    }

    /* The shell must survive terminal-generated job-control signals.
     * SIGTTOU is ignored before tcsetpgrp(): after setpgid() the shell
     * is briefly in a background group (when started by a parent without
     * job control, e.g. sh -c), and tcsetpgrp() would otherwise stop it. */
    if (signal(SIGINT, SIG_IGN) == SIG_ERR ||
        signal(SIGTSTP, SIG_IGN) == SIG_ERR ||
        signal(SIGQUIT, SIG_IGN) == SIG_ERR ||
        signal(SIGTTIN, SIG_IGN) == SIG_ERR ||
        signal(SIGTTOU, SIG_IGN) == SIG_ERR)
        return -1;

    /* Become a process-group leader when possible. */
    if (setpgid(0, 0) < 0 &&
        errno != EACCES && errno != EPERM)
        return -1;

    sh->shell_pgid = getpgrp();

    if (tcsetpgrp(STDIN_FILENO, sh->shell_pgid) < 0)
        return -1;

    return 0;
}

int main(int argc, char **argv)
{
    (void)argv;
    Shell sh = {0};
    sh.running = 1;

    if (initialize_terminal(&sh, argc > 1) < 0) {
        perror("simple_shell: terminal initialization");
        return 1;
    }

    if (install_child_signal_handler() < 0) {
        perror("simple_shell: SIGCHLD handler");
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
        reap_pending_children(sh.jobs);

        if (sh.interactive)
            prompt_render(&sh);

        errno = 0;

        if (read_command_line(sh.jobs, &line, &cap) < 0) {
            if (errno != 0)
                perror("simple_shell: input");
            else if (sh.interactive)
                putchar('\n');      /* Ctrl+D: end the prompt line */
            break;
        }

        /* BR-5: re-prompt without touching $? or history. */
        if (line_is_blank(line))
            continue;

        if (sh.history)
            history_add(sh.history, line);

        shell_execute_line(&sh, line);
    }

    if (sh.history && hist_path)
        history_save(sh.history, hist_path);

    history_destroy(sh.history);
    reap_pending_children(sh.jobs);
    job_table_destroy(sh.jobs);
    alias_destroy(sh.aliases);

    free(hist_path);
    free(line);

    return sh.last_status;
}
