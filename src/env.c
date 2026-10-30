#include "env.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;

int env_name_valid(const char *name)
{
    if (!name || !(*name == '_' ||
                   (*name >= 'A' && *name <= 'Z') ||
                   (*name >= 'a' && *name <= 'z')))
        return 0;
    for (const char *p = name + 1; *p; p++) {
        if (!(*p == '_' || (*p >= 'A' && *p <= 'Z') ||
              (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')))
            return 0;
    }
    return 1;
}

const char *env_get(const char *name)
{
    return env_name_valid(name) ? getenv(name) : NULL;
}

int env_set(const char *name, const char *value)
{
    if (!env_name_valid(name) || setenv(name, value, 1) != 0)
        return 1;
    return 0;
}

int env_unset(const char *name)
{
    if (!env_name_valid(name) || unsetenv(name) != 0)
        return 1;
    return 0;
}

static void not_valid(const char *builtin, const char *arg)
{
    sh_error(builtin, "'%s': not a valid identifier", arg);
}

static void print_environment(const char *prefix)
{
    for (char **e = environ; e && *e; e++)
        printf("%s%s\n", prefix, *e);
    fflush(stdout);
}

/* export NAME=value | export NAME | export (lists the environment). */
int builtin_export(Shell *sh, int argc, char **argv)
{
    (void)sh;
    int status = 0;

    if (argc == 1) {
        print_environment("export ");
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');

        if (!eq) {
            /* Every set variable is already exported; an unset NAME has
             * no value to export, so only the name is checked. */
            if (!env_name_valid(argv[i])) {
                not_valid("export", argv[i]);
                status = 1;
            }
            continue;
        }

        *eq = '\0';
        int valid = env_name_valid(argv[i]);
        int failed = valid && env_set(argv[i], eq + 1) != 0;
        *eq = '=';

        if (!valid) {
            not_valid("export", argv[i]);
            status = 1;
        } else if (failed) {
            sh_error("export", "%s: cannot set", argv[i]);
            status = 1;
        }
    }
    return status;
}

/* unset NAME... */
int builtin_unset(Shell *sh, int argc, char **argv)
{
    (void)sh;
    int status = 0;

    for (int i = 1; i < argc; i++) {
        if (!env_name_valid(argv[i])) {
            not_valid("unset", argv[i]);
            status = 1;
        } else if (env_unset(argv[i]) != 0) {
            sh_error("unset", "%s: cannot unset", argv[i]);
            status = 1;
        }
    }
    return status;
}

/* env: lists the environment, one NAME=value per line. */
int builtin_env(Shell *sh, int argc, char **argv)
{
    (void)sh;
    (void)argv;

    if (argc > 1) {
        sh_error("env", "too many arguments");
        return ST_SYNTAX;
    }
    print_environment("");
    return 0;
}
