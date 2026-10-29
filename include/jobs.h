#ifndef JOBS_H
#define JOBS_H

#include <stdio.h>
#include <sys/types.h>

typedef struct JobTable JobTable;

JobTable *job_table_create(void);
int job_table_add(JobTable *table, pid_t pid,
                  const char *command, int *job_id);
void job_table_reap(JobTable *table);
void job_table_print(const JobTable *table, FILE *stream);
void job_table_destroy(JobTable *table);

#endif
