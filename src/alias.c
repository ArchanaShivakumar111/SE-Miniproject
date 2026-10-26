#include "alias.h"

#include <stdlib.h>
#include <string.h>

#include "lexer.h"

#define ALIAS_MAX_DEPTH 16

AliasTable *alias_create(void)
{
    return calloc(1, sizeof(AliasTable));
}

void alias_destroy(AliasTable *t)
{
    if (!t)
        return;
    for (size_t i = 0; i < t->count; i++) {
        free(t->names[i]);
        free(t->values[i]);
    }
    free(t->names);
    free(t->values);
    free(t);
}

static long find(const AliasTable *t, const char *name)
{
    for (size_t i = 0; i < t->count; i++)
        if (strcmp(t->names[i], name) == 0)
            return (long)i;
    return -1;
}

const char *alias_get(const AliasTable *t, const char *name)
{
    long i = find(t, name);
    return i < 0 ? NULL : t->values[i];
}

int alias_set(AliasTable *t, const char *name, const char *value)
{
    char *v = strdup(value);
    if (!v)
        return -1;
    long i = find(t, name);
    if (i >= 0) {
        free(t->values[i]);
        t->values[i] = v;
        return 0;
    }
    char *n = strdup(name);
    char **nn = realloc(t->names, (t->count + 1) * sizeof(char *));
    if (nn)
        t->names = nn;
    char **nv = realloc(t->values, (t->count + 1) * sizeof(char *));
    if (nv)
        t->values = nv;
    if (!n || !nn || !nv) {
        free(n);
        free(v);
        return -1;
    }
    t->names[t->count] = n;
    t->values[t->count] = v;
    t->count++;
    return 0;
}

int alias_remove(AliasTable *t, const char *name)
{
    long i = find(t, name);
    if (i < 0)
        return -1;
    free(t->names[i]);
    free(t->values[i]);
    for (size_t j = (size_t)i + 1; j < t->count; j++) {
        t->names[j - 1] = t->names[j];
        t->values[j - 1] = t->values[j];
    }
    t->count--;
    return 0;
}

int alias_expand(const AliasTable *t, char ***argv, int *argc)
{
    const char *seen[ALIAS_MAX_DEPTH];
    int nseen = 0;

    while (*argc > 0 && nseen < ALIAS_MAX_DEPTH) {
        long idx = find(t, (*argv)[0]);
        if (idx < 0)
            break;
        const char *name = t->names[idx];
        int dup = 0;
        for (int i = 0; i < nseen; i++)
            if (seen[i] == name)
                dup = 1;
        if (dup)
            break;
        seen[nseen++] = name;

        char **exp = NULL;
        int en = lexer_tokenize(t->values[idx], &exp);
        if (en < 0)
            return -1;
        char **out = malloc((size_t)(en + *argc + 1) * sizeof(char *));
        if (!out) {
            lexer_free(exp);
            return -1;
        }
        int k = 0;
        for (int i = 0; i < en; i++)
            out[k++] = exp[i];
        for (int i = 1; i < *argc; i++)
            out[k++] = (*argv)[i];
        out[k] = NULL;
        free(exp);
        free((*argv)[0]);
        free(*argv);
        *argv = out;
        *argc = k;
    }
    return 0;
}
