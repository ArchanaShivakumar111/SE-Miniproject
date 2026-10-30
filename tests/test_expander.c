#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expander.h"
#include "lexer.h"

/* lex + expand + argv; checks the result against want (NULL-terminated). */
static void expect(const Shell *sh, const char *line, const char **want)
{
    TokenList *tl = lex(line, NULL);
    assert(tl);
    assert(expand(tl, sh) == 0);

    char **v;
    int n = token_list_to_argv(tl, &v);
    token_list_free(tl);

    int ok = 1, w = 0;
    for (; want[w]; w++)
        if (w >= n || strcmp(v[w], want[w]) != 0)
            ok = 0;
    if (!ok || n != w) {
        fprintf(stderr, "expand(%s): got", line);
        for (int i = 0; i < n; i++)
            fprintf(stderr, " [%s]", v[i]);
        fputc('\n', stderr);
        assert(0);
    }
    lexer_free(v);
}

#define EXPECT(sh, line, ...) expect(sh, line, (const char *[]){__VA_ARGS__, NULL})

int main(void)
{
    Shell sh = {0};

    setenv("HOME", "/home/t", 1);
    unsetenv("UNDEF");

    /* UT-EXP-01: $UNDEF produces no argument; '$HOME' stays literal. */
    EXPECT(&sh, "echo $HOME $UNDEF '$HOME'", "echo", "/home/t", "$HOME");

    /* UT-EXP-02: $? is the last status. */
    sh.last_status = 1;
    EXPECT(&sh, "echo $?", "echo", "1");
    sh.last_status = 127;
    EXPECT(&sh, "echo $? \"$?\" '$?' x$?y", "echo", "127", "127", "$?", "x127y");
    sh.last_status = 0;

    /* No word splitting (FR-003 deviation from bash). */
    setenv("X", "a b", 1);
    EXPECT(&sh, "ls $X", "ls", "a b");
    EXPECT(&sh, "ls \"$X\"", "ls", "a b");

    /* Quoted empty expansion stays as an empty argument. */
    EXPECT(&sh, "echo \"$UNDEF\" end", "echo", "", "end");
    EXPECT(&sh, "echo ''$UNDEF end", "echo", "", "end");
    EXPECT(&sh, "echo a$UNDEF", "echo", "a");

    /* Only the second $HOME expands; escapes and single quotes are literal. */
    EXPECT(&sh, "echo '$HOME'$HOME", "echo", "$HOME/home/t");
    EXPECT(&sh, "echo \\$HOME \"\\$HOME\" \"$HOME\"", "echo", "$HOME", "$HOME", "/home/t");

    /* Name boundaries. */
    EXPECT(&sh, "echo $HOME/x $HOME$HOME ${HOME} $HOMEx", "echo",
           "/home/t/x", "/home/t/home/t", "${HOME}");
    EXPECT(&sh, "echo $ $1 $- a$ \"$\"", "echo", "$", "$1", "$-", "a$", "$");
    EXPECT(&sh, "echo \"$\"HOME $\"HOME\"", "echo", "$HOME", "$HOME");

    /* SEC-002: values are inserted literally, never re-lexed or re-expanded. */
    setenv("V", "$(rm /tmp/canary)", 1);
    EXPECT(&sh, "echo $V", "echo", "$(rm /tmp/canary)");
    setenv("V", "a > b | c & $HOME ; 'q'", 1);
    EXPECT(&sh, "echo $V", "echo", "a > b | c & $HOME ; 'q'");

    /* Operators are untouched; an expanded word can be a redirect target.
     * An empty unquoted target vanishes, leaving > without a target, which
     * the executor reports as a syntax error. */
    {
        TokenList *tl = lex("cat <$HOME >$UNDEF", NULL);
        assert(tl && expand(tl, &sh) == 0);
        assert(tl->count == 4);
        assert(tl->items[1].type == T_LESS);
        assert(tl->items[2].type == T_WORD && strcmp(tl->items[2].text, "/home/t") == 0);
        assert(tl->items[3].type == T_GREAT);
        token_list_free(tl);
    }

    /* Empty list, and a line that expands to nothing. */
    {
        TokenList *tl = lex("", NULL);
        assert(tl && expand(tl, &sh) == 0 && tl->count == 0);
        token_list_free(tl);
        tl = lex("$UNDEF $UNDEF", NULL);
        assert(tl && expand(tl, &sh) == 0 && tl->count == 0);
        token_list_free(tl);
    }

    /* Long value (dynamic buffers). */
    {
        size_t n = 70000;
        char *big = malloc(n + 1);
        assert(big);
        memset(big, 'v', n);
        big[n] = '\0';
        setenv("BIG", big, 1);
        TokenList *tl = lex("x$BIG$BIG", NULL);
        assert(tl && expand(tl, &sh) == 0);
        assert(strlen(tl->items[0].text) == 2 * n + 1);
        token_list_free(tl);
        free(big);
    }

    puts("test_expander OK");
    return 0;
}
