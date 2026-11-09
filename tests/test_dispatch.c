/* NFR-001: the shell adds at most 2 ms of its own work to run a command.
 *
 * Overhead = (time for shell_execute_line("true") x RUNS) minus (time for
 * a bare fork + execvp + waitpid of the same program x RUNS), per command.
 * The best of several rounds is used so a busy machine does not fail it. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "alias.h"
#include "jobs.h"
#include "shell.h"

#define RUNS 200
#define ROUNDS 5
#define LIMIT_MS 2.0

static double now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static double bare_round(void)
{
    double start = now_ms();

    for (int i = 0; i < RUNS; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            char *argv[] = {"true", NULL};

            execvp("true", argv);
            _exit(127);
        }
        waitpid(pid, NULL, 0);
    }
    return now_ms() - start;
}

static double shell_round(Shell *sh)
{
    double start = now_ms();

    for (int i = 0; i < RUNS; i++)
        shell_execute_line(sh, "true");
    return now_ms() - start;
}

int main(void)
{
    Shell sh = {0};
    double best = 1e9;

    sh.running = 1;
    sh.aliases = alias_create();
    sh.jobs = job_table_create();

    for (int r = 0; r < ROUNDS; r++) {
        double bare = bare_round();
        double shell = shell_round(&sh);
        double overhead = (shell - bare) / RUNS;

        if (overhead < best)
            best = overhead;
    }

    job_table_destroy(sh.jobs);
    alias_destroy(sh.aliases);

    printf("dispatch overhead: %.3f ms per command (limit %.1f ms)\n",
           best, LIMIT_MS);
    if (best > LIMIT_MS) {
        fprintf(stderr, "FAIL: dispatch overhead above %.1f ms\n", LIMIT_MS);
        return 1;
    }
    puts("test_dispatch OK");
    return 0;
}
