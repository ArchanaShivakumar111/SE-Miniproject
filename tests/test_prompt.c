#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repl.h"

static void expect(const char *user, const char *host, const char *cwd,
                   const char *home, const char *want)
{
    char *got = prompt_format(user, host, cwd, home);

    assert(got);
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "prompt: got '%s', want '%s'\n", got, want);
        assert(0);
    }
    free(got);
}

int main(void)
{
    /* ST-REPL-01: user tester on host box, cwd ~/w. */
    expect("tester", "box", "/home/tester/w", "/home/tester", "tester@box:~/w$ ");
    expect("tester", "box", "/home/tester", "/home/tester", "tester@box:~$ ");
    expect("tester", "box", "/home/tester", "/home/tester/", "tester@box:~$ ");
    expect("tester", "box", "/home/tester/a/b", "/home/tester/", "tester@box:~/a/b$ ");

    /* Outside home, or a directory that only shares a prefix with it. */
    expect("tester", "box", "/tmp", "/home/tester", "tester@box:/tmp$ ");
    expect("tester", "box", "/home/testerx", "/home/tester", "tester@box:/home/testerx$ ");
    expect("tester", "box", "/", "/home/tester", "tester@box:/$ ");

    /* HOME=/ abbreviates only / itself. */
    expect("root", "box", "/", "/", "root@box:~$ ");
    expect("root", "box", "/tmp", "/", "root@box:/tmp$ ");

    /* No home: full path. */
    expect("tester", "box", "/home/tester", NULL, "tester@box:/home/tester$ ");
    expect("tester", "box", "/home/tester", "", "tester@box:/home/tester$ ");

    /* Lookup failures fall back to "$ ". */
    expect(NULL, "box", "/tmp", "/home/tester", "$ ");
    expect("tester", NULL, "/tmp", "/home/tester", "$ ");
    expect("tester", "box", NULL, "/home/tester", "$ ");
    expect("", "box", "/tmp", NULL, "$ ");

    /* BR-5 */
    assert(line_is_blank(""));
    assert(line_is_blank("\n"));
    assert(line_is_blank("  \t \r\n"));
    assert(!line_is_blank("  x \n"));

    puts("test_prompt OK");
    return 0;
}
