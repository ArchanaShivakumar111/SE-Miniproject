#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "alias.h"
#include "lexer.h"

static int expand(AliasTable *t, const char *line, char ***argv)
{
    int argc = lexer_tokenize(line, argv);
    assert(argc >= 0);
    assert(alias_expand(t, argv, &argc) == 0);
    return argc;
}

int main(void)
{
    AliasTable *t = alias_create();
    assert(alias_set(t, "ll", "ls -l") == 0);
    assert(strcmp(alias_get(t, "ll"), "ls -l") == 0);
    assert(alias_set(t, "ll", "ls -la") == 0 && t->count == 1);

    char **v;
    int n = expand(t, "ll /tmp", &v);
    assert(n == 3 && !strcmp(v[0], "ls") && !strcmp(v[1], "-la") && !strcmp(v[2], "/tmp"));
    lexer_free(v);

    alias_set(t, "ls", "ls -G");
    n = expand(t, "ll", &v);
    assert(n == 3 && !strcmp(v[0], "ls") && !strcmp(v[1], "-G") && !strcmp(v[2], "-la"));
    lexer_free(v);

    n = expand(t, "echo ll", &v);
    assert(n == 2 && !strcmp(v[1], "ll"));
    lexer_free(v);

    alias_set(t, "a", "b");
    alias_set(t, "b", "a");
    n = expand(t, "a", &v);
    assert(n == 1);
    lexer_free(v);

    assert(alias_remove(t, "ll") == 0);
    assert(alias_get(t, "ll") == NULL);
    assert(alias_remove(t, "ll") == -1);
    alias_destroy(t);
    puts("test_alias OK");
    return 0;
}
