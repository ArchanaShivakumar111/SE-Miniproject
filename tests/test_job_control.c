
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "jobs.h"

static pid_t spawn_stopped_child(int exit_after_continue)
{
    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0) {
        if (setpgid(0, 0) < 0)
            _exit(90);

        if (raise(SIGSTOP) != 0)
            _exit(91);

        if (exit_after_continue)
            _exit(42);

        for (;;)
            pause();
    }

    int group_result = setpgid(pid, pid);
    assert(group_result == 0);

    int status;
    pid_t result;

    do {
        result = waitpid(pid, &status, WUNTRACED);
    } while (result < 0 && errno == EINTR);

    assert(result == pid);
    assert(WIFSTOPPED(status));

    return pid;
}

static char *capture_builtin_output(
    Shell *sh, int argc, char **argv, int *result)
{
    int flush_result = fflush(stdout);
    assert(flush_result == 0);

    FILE *capture = tmpfile();
    assert(capture != NULL);

    int saved_stdout = dup(STDOUT_FILENO);
    assert(saved_stdout >= 0);

    int redirect_result = dup2(fileno(capture), STDOUT_FILENO);
    assert(redirect_result >= 0);

    *result = builtin_run(sh, argc, argv);

    flush_result = fflush(stdout);
    assert(flush_result == 0);

    int seek_result = fseek(capture, 0, SEEK_END);
    assert(seek_result == 0);

    long length = ftell(capture);
    assert(length >= 0);

    rewind(capture);

    char *output = calloc((size_t)length + 1, 1);
    assert(output != NULL);

    size_t bytes_read = fread(output, 1, (size_t)length, capture);
    output[bytes_read] = '\0';

    int restore_result = dup2(saved_stdout, STDOUT_FILENO);
    assert(restore_result >= 0);

    int close_result = close(saved_stdout);
    assert(close_result == 0);

    int file_close_result = fclose(capture);
    assert(file_close_result == 0);

    return output;
}

int main(void)
{
    Shell sh = {0};
    sh.running = 1;
    sh.interactive = 0;
    sh.jobs = job_table_create();
    assert(sh.jobs != NULL);

    assert(builtin_is("jobs"));
    assert(builtin_is("fg"));
    assert(builtin_is("bg"));

    assert(builtin_is_job_control("jobs"));
    assert(builtin_is_job_control("fg"));
    assert(builtin_is_job_control("bg"));
    assert(!builtin_is_job_control("echo"));

    /* Register a stopped child and verify the jobs output. */
    pid_t background_pid = spawn_stopped_child(0);
    int background_id;

    int add_result = job_table_add_stopped(
        sh.jobs, background_pid, "background-test", &background_id);
    assert(add_result == 0);

    int result;
    char *jobs_argv[] = {"jobs", NULL};

    char *output = capture_builtin_output(
        &sh, 1, jobs_argv, &result);

    assert(result == 0);
    assert(strstr(output, "[1] Stopped background-test") != NULL);
    free(output);

    /* bg resumes the stopped child without waiting for it. */
    char *bg_argv[] = {"bg", "%1", NULL};

    result = builtin_run(&sh, 2, bg_argv);
    assert(result == 0);

    JobInfo info;
    int find_result = job_table_find(
        sh.jobs, background_id, &info);
    assert(find_result == 0);
    assert(info.state == JOB_RUNNING);

    /* Stop and reap the child used to test bg. */
    int kill_result = kill(-background_pid, SIGTERM);
    assert(kill_result == 0);

    int status;
    pid_t waited;

    do {
        waited = waitpid(background_pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    assert(waited == background_pid);
    assert(WIFSIGNALED(status));

    job_table_reap(sh.jobs);

    find_result = job_table_find(
        sh.jobs, background_id, &info);
    assert(find_result < 0);

    /*
     * The next stopped child exits with status 42 after SIGCONT.
     * fg should resume it, wait for it, and remove it from the table.
     */
    pid_t foreground_pid = spawn_stopped_child(1);
    int foreground_id;

    add_result = job_table_add_stopped(
        sh.jobs, foreground_pid, "foreground-test", &foreground_id);
    assert(add_result == 0);

    char *fg_argv[] = {"fg", NULL};

    result = builtin_run(&sh, 1, fg_argv);
    assert(result == 42);

    find_result = job_table_find(
        sh.jobs, foreground_id, &info);
    assert(find_result < 0);

    job_table_destroy(sh.jobs);

    puts("test_job_control OK");
    return 0;
}
