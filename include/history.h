#ifndef HISTORY_H
#define HISTORY_H

#include <stddef.h>

#define HISTORY_MAX 1000

typedef struct History {
    char *entries[HISTORY_MAX];
    size_t start;
    size_t count;
} History;

History *history_create(void);
void history_destroy(History *h);
int history_add(History *h, const char *line);
size_t history_count(const History *h);
/* index 0 is the oldest retained entry */
const char *history_get(const History *h, size_t index);

int history_load(History *h, const char *path);
int history_save(const History *h, const char *path);
/* Returns malloc'd "$HOME/.simple_shell_history" or NULL. */
char *history_default_path(void);

#endif
