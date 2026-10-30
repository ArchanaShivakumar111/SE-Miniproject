#ifndef HISTORY_H
#define HISTORY_H

#include "shell.h"

/* FR-010 / SEC-006: ring buffer (HISTORY_SIZE) persisted to a 0600 file. */
struct History;

struct History *history_create(void);
void            history_add(struct History *h, const char *line);
void            history_destroy(struct History *h);

#endif /* HISTORY_H */
