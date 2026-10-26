#include "builtins.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "alias.h"

typedef int (*builtin_fn)(Shell *, int, char **);

static int bi_cd(Shell *sh, int argc, char **argv)
{
    (void)sh;
    const char *target;
    if (argc > 2) {
        fprintf(stderr, "simple_shell: cd: too many arguments\n");
        return 1;
    }
    if (argc == 1) {
        target = getenv("HOME");
        if (!target) {
            fprintf(stderr, "simple_shell: cd: HOME not set\n");
            return 1;
        }
    } else {
        target = argv[1];
    }
    if (chdir(target) < 0) {
        fprintf(stderr, "simple_shell: cd: %s: %s\n", target, strerror(errno));
        return 1;
    }
    return 0;
}

static int bi_pwd(Shell *sh, int argc, char **argv)
{
    (void)sh; (void)argc; (void)argv;
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) {
        fprintf(stderr, "simple_shell: pwd: %s\n", strerror(errno));
        return 1;
    }
    printf("%s\n", cwd);
    fflush(stdout);
    return 0;
}

static int bi_echo(Shell *sh, int argc, char **argv)
{
    (void)sh;
    int i = 1, newline = 1;
    if (i < argc && strcmp(argv[i], "-n") == 0) {
        newline = 0;
        i++;
    }
    for (; i < argc; i++) {
        fputs(argv[i], stdout);
        if (i + 1 < argc)
            fputc(' ', stdout);
    }
    if (newline)
        fputc('\n', stdout);
    fflush(stdout);
    return 0;
}

static int bi_exit(Shell *sh, int argc, char **argv)
{
    int code = sh->last_status;
    if (argc > 2) {
        fprintf(stderr, "simple_shell: exit: too many arguments\n");
        return 1;
    }
    if (argc == 2) {
        char *end;
        errno = 0;
        long v = strtol(argv[1], &end, 10);
        if (errno || *end || end == argv[1]) {
            fprintf(stderr, "simple_shell: exit: %s: numeric argument required\n", argv[1]);
            code = 2;
        } else {
            code = (int)(v & 0xff);
        }
    }
    sh->last_status = code;
    sh->running = 0;
    return code;
}

static int bi_alias(Shell *sh, int argc, char **argv)
{
    AliasTable *t = sh->aliases;
    if (!t)
        return 1;
    if (argc == 1) {
        for (size_t i = 0; i < t->count; i++)
            printf("alias %s='%s'\n", t->names[i], t->values[i]);
        fflush(stdout);
        return 0;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!eq) {
            const char *v = alias_get(t, argv[i]);
            if (v) {
                printf("alias %s='%s'\n", argv[i], v);
            } else {
                fprintf(stderr, "simple_shell: alias: %s: not found\n", argv[i]);
                rc = 1;
            }
        } else if (eq == argv[i]) {
            fprintf(stderr, "simple_shell: alias: invalid alias name\n");
            rc = 1;
        } else {
            *eq = '\0';
            if (alias_set(t, argv[i], eq + 1) < 0)
                rc = 1;
            *eq = '=';
        }
    }
    fflush(stdout);
    return rc;
}

static int bi_unalias(Shell *sh, int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "simple_shell: unalias: usage: unalias name [name ...]\n");
        return 2;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (!sh->aliases || alias_remove(sh->aliases, argv[i]) < 0) {
            fprintf(stderr, "simple_shell: unalias: %s: not found\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}

static const struct { const char *name; builtin_fn fn; } table[] = {
    {"cd", bi_cd}, {"pwd", bi_pwd}, {"echo", bi_echo}, {"exit", bi_exit},
    {"alias", bi_alias}, {"unalias", bi_unalias},
};

int builtin_is(const char *name)
{
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++)
        if (strcmp(table[i].name, name) == 0)
            return 1;
    return 0;
}

int builtin_run(Shell *sh, int argc, char **argv)
{
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++)
        if (strcmp(table[i].name, argv[0]) == 0)
            return table[i].fn(sh, argc, argv);
    return 127;
}
