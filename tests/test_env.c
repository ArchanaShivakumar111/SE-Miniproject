#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "env.h"

/* Runs a built-in on heap copies of argv, as the shell does (export
 * writes into its arguments temporarily). */
static int run(Shell *sh, char **argv)
{
    char *copy[8];
    int argc = 0;

    while (argv[argc]) {
        copy[argc] = strdup(argv[argc]);
        assert(copy[argc]);
        argc++;
    }
    copy[argc] = NULL;

    int status = builtin_run(sh, argc, copy);

    for (int i = 0; i < argc; i++)
        free(copy[i]);
    return status;
}

int main(void)
{
    Shell sh = {0};
    sh.running = 1;

    assert(env_name_valid("X") && env_name_valid("_a1") && env_name_valid("PATH"));
    assert(!env_name_valid("1A") && !env_name_valid("") && !env_name_valid("A-B"));
    assert(!env_name_valid("A=B") && !env_name_valid(NULL));

    assert(builtin_is("export") && builtin_is("unset") && builtin_is("env"));

    /* export NAME=value; the value may contain = and spaces. */
    unsetenv("UT_X");
    assert(run(&sh, (char *[]){"export", "UT_X=5", NULL}) == 0);
    assert(env_get("UT_X") && strcmp(env_get("UT_X"), "5") == 0);
    char arg[] = "UT_Y=a=b c";
    char *argv_y[] = {"export", arg, NULL};
    assert(builtin_run(&sh, 2, argv_y) == 0);
    assert(strcmp(getenv("UT_Y"), "a=b c") == 0);
    assert(strcmp(arg, "UT_Y=a=b c") == 0);        /* argv restored */

    /* export NAME: valid name, nothing set if it was unset. */
    unsetenv("UT_Z");
    assert(run(&sh, (char *[]){"export", "UT_Z", NULL}) == 0);
    assert(getenv("UT_Z") == NULL);
    assert(run(&sh, (char *[]){"export", "UT_X", NULL}) == 0);
    assert(strcmp(getenv("UT_X"), "5") == 0);

    /* Invalid identifiers: status 1, nothing changed, later names still set. */
    assert(run(&sh, (char *[]){"export", "1A=5", NULL}) == 1);
    assert(getenv("1A") == NULL);
    assert(run(&sh, (char *[]){"export", "1A", NULL}) == 1);
    assert(run(&sh, (char *[]){"export", "=x", "UT_W=1", NULL}) == 1);
    assert(strcmp(getenv("UT_W"), "1") == 0);

    /* unset NAME... */
    assert(run(&sh, (char *[]){"unset", "UT_X", "UT_Y", "UT_NEVER_SET", NULL}) == 0);
    assert(getenv("UT_X") == NULL && getenv("UT_Y") == NULL);
    assert(run(&sh, (char *[]){"unset", "9x", "UT_W", NULL}) == 1);
    assert(getenv("UT_W") == NULL);

    /* env takes no arguments. */
    assert(run(&sh, (char *[]){"env", "extra", NULL}) == 2);

    puts("test_env OK");
    return 0;
}
