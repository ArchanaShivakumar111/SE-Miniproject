#ifndef ALIAS_H
#define ALIAS_H

#include <stddef.h>

typedef struct AliasTable {
    char **names;
    char **values;
    size_t count;
} AliasTable;

AliasTable *alias_create(void);
void alias_destroy(AliasTable *t);
int alias_set(AliasTable *t, const char *name, const char *value);
const char *alias_get(const AliasTable *t, const char *name);
int alias_remove(AliasTable *t, const char *name);

/* Replaces a leading alias in *argv (NULL-terminated, *argc tokens) with its
 * expansion. An alias is expanded at most once per line, so self-referential
 * aliases such as ls='ls -l' terminate. Returns 0 or -1 on error. */
int alias_expand(const AliasTable *t, char ***argv, int *argc);

#endif
