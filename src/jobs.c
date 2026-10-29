#include "jobs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

typedef struct Job {
    int id;
    pid_t pid;
    char *command;
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

int job_table_add(JobTable *table, pid_t pid,
                  const char *command, int *job_id)
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

    Job **tail = &table->head;
    while (*tail)
        tail = &(*tail)->next;
    *tail = job;

    *job_id = job->id;
    return 0;
}

void job_table_reap(JobTable *table)
{
    if (!table)
        return;

    Job **cursor = &table->head;

    while (*cursor) {
        Job *job = *cursor;
        int status;
        pid_t result = waitpid(job->pid, &status, WNOHANG);

        if (result == job->pid ||
            (result < 0 && errno == ECHILD)) {
            *cursor = job->next;
            free(job->command);
            free(job);
        } else {
            cursor = &job->next;
        }
    }
}

void job_table_print(const JobTable *table, FILE *stream)
{
    if (!table || !stream)
        return;

    for (const Job *job = table->head; job; job = job->next)
        fprintf(stream, "[%d] %ld %s\n",
                job->id, (long)job->pid, job->command);
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
