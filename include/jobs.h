#ifndef JOBS_H
#define JOBS_H

#include "shell.h"

/* FR-007 / FR-008 / FR-009c: background jobs and job table. */
struct JobTable;

struct JobTable *jobs_create(void);
void             jobs_destroy(struct JobTable *t);

#endif /* JOBS_H */
