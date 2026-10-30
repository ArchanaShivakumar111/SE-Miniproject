
#include "jobs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

typedef struct Job {
    int id;
    pid_t pid;
    char *command;
    JobState state;
    struct Job *next;
} Job;

struct JobTable {
    int next_id;
    Job *head;
};

JobTable *job_table_create(void)
{
    JobTable *table = calloc(1, sizeof(*table));

    if (table)
        table->next_id = 1;

    return table;
}

static int job_table_add_with_state(
    JobTable *table, pid_t pid, const char *command,
    int *job_id, JobState state)
{
    if (!table || pid <= 0 || !command || !job_id)
        return -1;

    Job *job = calloc(1, sizeof(*job));
    if (!job)
        return -1;

    size_t len = strlen(command);

    while (len > 0 &&
           (command[len - 1] == '\n' ||
            command[len - 1] == '\r' ||
            command[len - 1] == ' ' ||
            command[len - 1] == '\t'))
        len--;

    job->command = malloc(len + 1);
    if (!job->command) {
        free(job);
        return -1;
    }

    memcpy(job->command, command, len);
    job->command[len] = '\0';

    job->id = table->next_id++;
    if (table->next_id <= 0)
        table->next_id = 1;

    job->pid = pid;
    job->state = state;

    Job **tail = &table->head;
    while (*tail)
        tail = &(*tail)->next;

    *tail = job;
    *job_id = job->id;

    return 0;
}

int job_table_add(JobTable *table, pid_t pid,
                  const char *command, int *job_id)
{
    return job_table_add_with_state(
        table, pid, command, job_id, JOB_RUNNING);
}

int job_table_add_stopped(JobTable *table, pid_t pid,
                          const char *command, int *job_id)
{
    return job_table_add_with_state(
        table, pid, command, job_id, JOB_STOPPED);
}

int job_table_find(const JobTable *table, int job_id,
                   JobInfo *info)
{
    if (!table || !info || job_id <= 0)
        return -1;

    for (const Job *job = table->head; job; job = job->next) {
        if (job->id == job_id) {
            info->id = job->id;
            info->pid = job->pid;
            info->command = job->command;
            info->state = job->state;
            return 0;
        }
    }

    return -1;
}

int job_table_latest_id(const JobTable *table, int *job_id)
{
    if (!table || !job_id || !table->head)
        return -1;

    const Job *job = table->head;

    while (job->next)
        job = job->next;

    *job_id = job->id;
    return 0;
}

int job_table_set_state(JobTable *table, int job_id,
                        JobState state)
{
    if (!table || job_id <= 0)
        return -1;

    for (Job *job = table->head; job; job = job->next) {
        if (job->id == job_id) {
            job->state = state;
            return 0;
        }
    }

    return -1;
}

int job_table_remove(JobTable *table, int job_id)
{
    if (!table || job_id <= 0)
        return -1;

    Job **cursor = &table->head;

    while (*cursor) {
        Job *job = *cursor;

        if (job->id == job_id) {
            *cursor = job->next;
            free(job->command);
            free(job);
            return 0;
        }

        cursor = &job->next;
    }

    return -1;
}

void job_table_reap(JobTable *table)
{
    if (!table)
        return;

    Job **cursor = &table->head;

    while (*cursor) {
        Job *job = *cursor;
        int status;

        pid_t result = waitpid(
            job->pid, &status,
            WNOHANG | WUNTRACED | WCONTINUED);

        if (result == job->pid) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                *cursor = job->next;
                free(job->command);
                free(job);
                continue;
            }

            if (WIFSTOPPED(status))
                job->state = JOB_STOPPED;
            else if (WIFCONTINUED(status))
                job->state = JOB_RUNNING;
        } else if (result < 0 && errno == ECHILD) {
            *cursor = job->next;
            free(job->command);
            free(job);
            continue;
        }

        cursor = &job->next;
    }
}

void job_table_print(const JobTable *table, FILE *stream)
{
    if (!table || !stream)
        return;

    for (const Job *job = table->head; job; job = job->next) {
        fprintf(stream, "[%d] %ld %s\n",
                job->id, (long)job->pid, job->command);
    }
}

void job_table_print_status(const JobTable *table, FILE *stream)
{
    if (!table || !stream)
        return;

    for (const Job *job = table->head; job; job = job->next) {
        const char *state =
            job->state == JOB_STOPPED ? "Stopped" : "Running";

        fprintf(stream, "[%d] %-7s %s\n",
                job->id, state, job->command);
    }
}

void job_table_destroy(JobTable *table)
{
    if (!table)
        return;

    Job *job = table->head;

    while (job) {
        Job *next = job->next;
        free(job->command);
        free(job);
        job = next;
    }

    free(table);
}
