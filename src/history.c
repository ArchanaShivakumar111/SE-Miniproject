#include "history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

History *history_create(void)
{
    return calloc(1, sizeof(History));
}

void history_destroy(History *h)
{
    if (!h)
        return;
    for (size_t i = 0; i < h->count; i++)
        free(h->entries[(h->start + i) % HISTORY_MAX]);
    free(h);
}

int history_add(History *h, const char *line)
{
    char *copy = strdup(line);
    if (!copy)
        return -1;
    size_t n = strlen(copy);
    while (n > 0 && (copy[n - 1] == '\n' || copy[n - 1] == '\r'))
        copy[--n] = '\0';
    if (n == 0) {
        free(copy);
        return 0;
    }
    if (h->count == HISTORY_MAX) {
        free(h->entries[h->start]);
        h->entries[h->start] = copy;
        h->start = (h->start + 1) % HISTORY_MAX;
    } else {
        h->entries[(h->start + h->count) % HISTORY_MAX] = copy;
        h->count++;
    }
    return 0;
}

size_t history_count(const History *h)
{
    return h->count;
}

const char *history_get(const History *h, size_t index)
{
    if (index >= h->count)
        return NULL;
    return h->entries[(h->start + index) % HISTORY_MAX];
}

int history_load(History *h, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    char *line = NULL;
    size_t cap = 0;
    int rc = 0;
    while (getline(&line, &cap, f) >= 0) {
        if (history_add(h, line) < 0) {
            rc = -1;
            break;
        }
    }
    free(line);
    fclose(f);
    return rc;
}

int history_save(const History *h, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    for (size_t i = 0; i < h->count; i++)
        fprintf(f, "%s\n", history_get(h, i));
    return fclose(f) == 0 ? 0 : -1;
}

char *history_default_path(void)
{
    const char *home = getenv("HOME");
    if (!home)
        return NULL;
    size_t n = strlen(home) + sizeof "/.simple_shell_history";
    char *p = malloc(n);
    if (p)
        snprintf(p, n, "%s/.simple_shell_history", home);
    return p;
}
