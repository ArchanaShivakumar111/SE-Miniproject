
#include "builtins.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "alias.h"
#include "jobs.h"

typedef int (*builtin_fn)(Shell *, int, char **);

static int bi_cd(Shell *sh, int argc, char **argv)
{
    (void)sh;
    const char *target;

    if (argc > 2) {
        fprintf(stderr, "simple_shell: cd: too many arguments\n");
        return 1;
    }

    if (argc == 1) {
        target = getenv("HOME");
        if (!target) {
            fprintf(stderr, "simple_shell: cd: HOME not set\n");
            return 1;
        }
    } else {
        target = argv[1];
    }

    if (chdir(target) < 0) {
        fprintf(stderr, "simple_shell: cd: %s: %s\n",
                target, strerror(errno));
        return 1;
    }

    return 0;
}

static int bi_pwd(Shell *sh, int argc, char **argv)
{
    (void)sh;
    (void)argc;
    (void)argv;

    char cwd[PATH_MAX];

    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "simple_shell: pwd: %s\n", strerror(errno));
        return 1;
    }

    printf("%s\n", cwd);
    fflush(stdout);
    return 0;
}

static int bi_echo(Shell *sh, int argc, char **argv)
{
    (void)sh;
    int i = 1;
    int newline = 1;

    if (i < argc && strcmp(argv[i], "-n") == 0) {
        newline = 0;
        i++;
    }

    for (; i < argc; i++) {
        fputs(argv[i], stdout);
        if (i + 1 < argc)
            fputc(' ', stdout);
    }

    if (newline)
        fputc('\n', stdout);

    fflush(stdout);
    return 0;
}

static int bi_exit(Shell *sh, int argc, char **argv)
{
    int code = sh->last_status;

    if (argc > 2) {
        fprintf(stderr, "simple_shell: exit: too many arguments\n");
        return 1;
    }

    if (argc == 2) {
        char *end;
        errno = 0;
        long value = strtol(argv[1], &end, 10);

        if (errno || *end || end == argv[1]) {
            fprintf(stderr,
                    "simple_shell: exit: %s: numeric argument required\n",
                    argv[1]);
            code = 2;
        } else {
            code = (int)(value & 0xff);
        }
    }

    sh->last_status = code;
    sh->running = 0;
    return code;
}

static int bi_alias(Shell *sh, int argc, char **argv)
{
    AliasTable *alias_table = sh->aliases;

    if (!alias_table)
        return 1;

    if (argc == 1) {
        for (size_t i = 0; i < alias_table->count; i++) {
            printf("alias %s='%s'\n",
                   alias_table->names[i], alias_table->values[i]);
        }

        fflush(stdout);
        return 0;
    }

    int result = 0;

    for (int i = 1; i < argc; i++) {
        char *equal = strchr(argv[i], '=');

        if (!equal) {
            const char *value = alias_get(alias_table, argv[i]);

            if (value) {
                printf("alias %s='%s'\n", argv[i], value);
            } else {
                fprintf(stderr,
                        "simple_shell: alias: %s: not found\n",
                        argv[i]);
                result = 1;
            }
        } else if (equal == argv[i]) {
            fprintf(stderr,
                    "simple_shell: alias: invalid alias name\n");
            result = 1;
        } else {
            *equal = '\0';

            if (alias_set(alias_table, argv[i], equal + 1) < 0)
                result = 1;

            *equal = '=';
        }
    }

    fflush(stdout);
    return result;
}

static int bi_unalias(Shell *sh, int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
                "simple_shell: unalias: usage: unalias name [name ...]\n");
        return 2;
    }

    int result = 0;

    for (int i = 1; i < argc; i++) {
        if (!sh->aliases || alias_remove(sh->aliases, argv[i]) < 0) {
            fprintf(stderr,
                    "simple_shell: unalias: %s: not found\n",
                    argv[i]);
            result = 1;
        }
    }

    return result;
}

static int bi_jobs(Shell *sh, int argc, char **argv)
{
    (void)argv;

    if (argc != 1) {
        fprintf(stderr, "simple_shell: jobs: usage: jobs\n");
        return 2;
    }

    if (!sh->jobs)
        return 1;

    job_table_reap(sh->jobs);
    job_table_print_status(sh->jobs, stdout);
    fflush(stdout);

    return 0;
}

/*
 * Accept job IDs as either 1 or %1.
 * With no argument, select the latest job.
 */
static int resolve_job_id(
    const Shell *sh, const char *name,
    int argc, char **argv, int *job_id)
{
    if (argc > 2) {
        fprintf(stderr,
                "simple_shell: %s: usage: %s [%%job_id]\n",
                name, name);
        return -1;
    }

    if (!sh->jobs) {
        fprintf(stderr, "simple_shell: %s: no current job\n", name);
        return -1;
    }

    if (argc == 1) {
        if (job_table_latest_id(sh->jobs, job_id) < 0) {
            fprintf(stderr,
                    "simple_shell: %s: no current job\n", name);
            return -1;
        }

        return 0;
    }

    const char *spec = argv[1];

    if (spec[0] == '%')
        spec++;

    errno = 0;
    char *end = NULL;
    long value = strtol(spec, &end, 10);

    if (spec[0] == '\0' || errno != 0 ||
        end == spec || *end != '\0' ||
        value <= 0 || value > INT_MAX) {
        fprintf(stderr,
                "simple_shell: %s: invalid job ID: %s\n",
                name, argv[1]);
        return -1;
    }

    *job_id = (int)value;
    return 0;
}

static void restore_shell_terminal(Shell *sh)
{
    if (sh->interactive &&
        tcsetpgrp(STDIN_FILENO, sh->shell_pgid) < 0) {
        perror("simple_shell: restore terminal");
    }
}

static int bi_fg(Shell *sh, int argc, char **argv)
{
    if (argc > 2) {
        fprintf(stderr,
                "simple_shell: fg: usage: fg [%%job_id]\n");
        return 2;
    }

    if (!sh->jobs) {
        fprintf(stderr, "simple_shell: fg: no current job\n");
        return 1;
    }

    job_table_reap(sh->jobs);

    int job_id;

    if (resolve_job_id(sh, "fg", argc, argv, &job_id) < 0)
        return 1;

    JobInfo job;

    if (job_table_find(sh->jobs, job_id, &job) < 0) {
        fprintf(stderr,
                "simple_shell: fg: no such job: %d\n", job_id);
        return 1;
    }

    if (sh->interactive &&
        tcsetpgrp(STDIN_FILENO, job.pid) < 0) {
        perror("simple_shell: fg: terminal");
        return 1;
    }

    if (job.state == JOB_STOPPED) {
        if (kill(-job.pid, SIGCONT) < 0) {
            int saved_errno = errno;
            restore_shell_terminal(sh);
            errno = saved_errno;
            perror("simple_shell: fg: SIGCONT");
            return 1;
        }

        (void)job_table_set_state(
            sh->jobs, job.id, JOB_RUNNING);
    }

    int status;
    pid_t result;

    do {
        result = waitpid(job.pid, &status, WUNTRACED);
    } while (result < 0 && errno == EINTR);

    restore_shell_terminal(sh);

    if (result < 0) {
        perror("simple_shell: fg: waitpid");
        job_table_reap(sh->jobs);
        return 1;
    }

    if (WIFSTOPPED(status)) {
        (void)job_table_set_state(
            sh->jobs, job.id, JOB_STOPPED);

        printf("[%d] Stopped %s\n", job.id, job.command);
        fflush(stdout);

        return 128 + WSTOPSIG(status);
    }

    if (WIFEXITED(status)) {
        (void)job_table_remove(sh->jobs, job.id);
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        (void)job_table_remove(sh->jobs, job.id);
        return 128 + WTERMSIG(status);
    }

    return 1;
}

static int bi_bg(Shell *sh, int argc, char **argv)
{
    if (!sh->jobs) {
        fprintf(stderr, "simple_shell: bg: no current job\n");
        return 1;
    }

    job_table_reap(sh->jobs);

    int job_id;

    if (resolve_job_id(sh, "bg", argc, argv, &job_id) < 0)
        return 1;

    JobInfo job;

    if (job_table_find(sh->jobs, job_id, &job) < 0) {
        fprintf(stderr,
                "simple_shell: bg: no such job: %d\n", job_id);
        return 1;
    }

    if (job.state == JOB_STOPPED) {
        if (kill(-job.pid, SIGCONT) < 0) {
            perror("simple_shell: bg: SIGCONT");
            job_table_reap(sh->jobs);
            return 1;
        }

        if (job_table_set_state(
                sh->jobs, job.id, JOB_RUNNING) < 0) {
            fprintf(stderr,
                    "simple_shell: bg: could not update job state\n");
            return 1;
        }
    }

    printf("[%d] %s &\n", job.id, job.command);
    fflush(stdout);

    return 0;
}

static const struct {
    const char *name;
    builtin_fn fn;
} table[] = {
    {"cd", bi_cd},
    {"pwd", bi_pwd},
    {"echo", bi_echo},
    {"exit", bi_exit},
    {"alias", bi_alias},
    {"unalias", bi_unalias},
    {"jobs", bi_jobs},
    {"fg", bi_fg},
    {"bg", bi_bg}
};

int builtin_is(const char *name)
{
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (strcmp(table[i].name, name) == 0)
            return 1;
    }

    return 0;
}

int builtin_is_job_control(const char *name)
{
    return strcmp(name, "jobs") == 0 ||
           strcmp(name, "fg") == 0 ||
           strcmp(name, "bg") == 0;
}

int builtin_run(Shell *sh, int argc, char **argv)
{
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (strcmp(table[i].name, argv[0]) == 0)
            return table[i].fn(sh, argc, argv);
    }

    return 127;
}
