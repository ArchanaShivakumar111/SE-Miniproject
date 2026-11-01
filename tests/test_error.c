#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"

/* Runs sh_error with stderr redirected to a pipe and returns what it wrote. */
static char *capture(const char *ctx, const char *arg)
{
    int fds[2];
    assert(pipe(fds) == 0);

    int saved = dup(STDERR_FILENO);
    assert(saved >= 0 && dup2(fds[1], STDERR_FILENO) >= 0);
    close(fds[1]);

    if (arg)
        sh_error(ctx, "%s: %d", arg, 42);
    else
        sh_error(ctx, "plain");

    assert(dup2(saved, STDERR_FILENO) >= 0);
    close(saved);

    static char buf[256];
    ssize_t n = read(fds[0], buf, sizeof buf - 1);
    close(fds[0]);
    assert(n >= 0);
    buf[n] = '\0';
    return buf;
}

int main(void)
{
    assert(strcmp(capture("cd", "dir"), "simple_shell: cd: dir: 42\n") == 0);
    assert(strcmp(capture(NULL, NULL), "simple_shell: plain\n") == 0);
    assert(strcmp(capture("syntax error", NULL),
                  "simple_shell: syntax error: plain\n") == 0);

    assert(ST_GENERAL == 1 && ST_SYNTAX == 2 && ST_NOEXEC == 126 &&
           ST_NOTFOUND == 127 && ST_SIGNAL_BASE == 128);

    puts("test_error OK");
    return 0;
}
