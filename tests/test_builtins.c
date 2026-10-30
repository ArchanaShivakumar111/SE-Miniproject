#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"

int main(void)
{
    Shell sh = {0};
    sh.running = 1;
    char cwd[PATH_MAX];

    assert(builtin_is("cd") && builtin_is("pwd") && builtin_is("echo") && builtin_is("exit"));
    assert(!builtin_is("ls"));

    char *cd_tmp[] = {"cd", "/tmp", NULL};
    assert(builtin_run(&sh, 2, cd_tmp) == 0);
    assert(getcwd(cwd, sizeof cwd) && (strstr(cwd, "tmp") != NULL));

    char *cd_bad[] = {"cd", "/no/such/dir", NULL};
    assert(builtin_run(&sh, 2, cd_bad) == 1);

    char *cd_many[] = {"cd", "a", "b", NULL};
    assert(builtin_run(&sh, 3, cd_many) == 1);

    char *pwd[] = {"pwd", NULL};
    assert(builtin_run(&sh, 1, pwd) == 0);

    char *echo[] = {"echo", "-n", "hi", NULL};
    assert(builtin_run(&sh, 3, echo) == 0);

    char *ex[] = {"exit", "3", NULL};
    assert(builtin_run(&sh, 2, ex) == 3);
    assert(sh.running == 0 && sh.last_status == 3);

    puts("test_builtins OK");
    return 0;
}
