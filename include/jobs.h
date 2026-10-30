
#ifndef JOBS_H
#define JOBS_H

#include <stdio.h>
#include <sys/types.h>

typedef struct JobTable JobTable;

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED
} JobState;

typedef struct {
    int id;
    pid_t pid;
    const char *command;
    JobState state;
} JobInfo;

JobTable *job_table_create(void);

int job_table_add(JobTable *table, pid_t pid,
                  const char *command, int *job_id);

int job_table_add_stopped(JobTable *table, pid_t pid,
                          const char *command, int *job_id);

int job_table_find(const JobTable *table, int job_id,
                   JobInfo *info);

int job_table_latest_id(const JobTable *table, int *job_id);

int job_table_set_state(JobTable *table, int job_id,
                        JobState state);

int job_table_remove(JobTable *table, int job_id);

void job_table_reap(JobTable *table);
void job_table_print(const JobTable *table, FILE *stream);
void job_table_print_status(const JobTable *table, FILE *stream);
void job_table_destroy(JobTable *table);

#endif
