#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

static void expect_word(const TokenList *tl, size_t i, const char *text)
{
    assert(i < tl->count);
    assert(tl->items[i].type == T_WORD);
    assert(strcmp(tl->items[i].text, text) == 0);
    assert(tl->items[i].qmask != NULL);
}

static void expect_op(const TokenList *tl, size_t i, TokenType type)
{
    assert(i < tl->count);
    assert(tl->items[i].type == type);
    assert(tl->items[i].qmask == NULL);
}

/* UT-LEX-01: quotes and escapes. */
static void test_quotes_and_escapes(void)
{
    ShError err;
    TokenList *tl = lex("echo \"a b\" 'c $d' e\\ f", &err);

    assert(tl && err.code == ERR_NONE);
    assert(tl->count == 4);
    expect_word(tl, 0, "echo");
    expect_word(tl, 1, "a b");
    expect_word(tl, 2, "c $d");
    expect_word(tl, 3, "e f");

    /* qmask: "a b" double-quoted, 'c $d' single-quoted, e\ f escaped space. */
    for (int i = 0; i < 3; i++)
        assert(tl->items[1].qmask[i] == Q_DOUBLE);
    for (int i = 0; i < 4; i++)
        assert(tl->items[2].qmask[i] == Q_SINGLE);
    assert(tl->items[3].qmask[0] == Q_NONE);
    assert(tl->items[3].qmask[1] == Q_SINGLE);
    assert(tl->items[3].qmask[2] == Q_NONE);
    token_list_free(tl);
}

/* UT-LEX-02: unterminated quote is a syntax error. */
static void test_unterminated(void)
{
    ShError err;

    assert(lex("echo \"abc", &err) == NULL);
    assert(err.code == ERR_SYNTAX);
    assert(strcmp(err.msg, "unterminated quote") == 0);

    assert(lex("echo 'abc\n", &err) == NULL && err.code == ERR_SYNTAX);
    assert(lex("echo \"a\\\"", &err) == NULL && err.code == ERR_SYNTAX);
    assert(lex("'", NULL) == NULL);     /* err may be NULL */
}

/* UT-LEX-03: operators recognised without spaces. */
static void test_operators_no_spaces(void)
{
    ShError err;
    TokenList *tl = lex("ls>o 2>e|wc&", &err);

    assert(tl && tl->count == 8);
    expect_word(tl, 0, "ls");
    expect_op(tl, 1, T_GREAT);
    expect_word(tl, 2, "o");
    expect_op(tl, 3, T_ERR_GREAT);
    expect_word(tl, 4, "e");
    expect_op(tl, 5, T_PIPE);
    expect_word(tl, 6, "wc");
    expect_op(tl, 7, T_AMP);
    token_list_free(tl);
}

static void test_all_operators(void)
{
    TokenList *tl = lex("a<b>>c&>d | e > f 2> g &", NULL);
    TokenType want[] = {T_WORD, T_LESS, T_WORD, T_DGREAT, T_WORD,
                        T_AND_GREAT, T_WORD, T_PIPE, T_WORD, T_GREAT,
                        T_WORD, T_ERR_GREAT, T_WORD, T_AMP};

    assert(tl && tl->count == sizeof want / sizeof want[0]);
    for (size_t i = 0; i < tl->count; i++)
        assert(tl->items[i].type == want[i]);
    assert(strcmp(tl->items[3].text, ">>") == 0);
    assert(strcmp(tl->items[5].text, "&>") == 0);
    token_list_free(tl);

    /* "2>" only at the start of a word; quoted operators are words. */
    tl = lex("a2>f '|' \">\" \\&", NULL);
    assert(tl && tl->count == 6);
    expect_word(tl, 0, "a2");
    expect_op(tl, 1, T_GREAT);
    expect_word(tl, 2, "f");
    expect_word(tl, 3, "|");
    expect_word(tl, 4, ">");
    expect_word(tl, 5, "&");
    token_list_free(tl);
}

static void test_double_quote_escapes(void)
{
    /* Inside "..." a backslash escapes only $ " and \. */
    TokenList *tl = lex("\"\\$x \\\" \\\\ \\n\" $y", NULL);

    assert(tl && tl->count == 2);
    expect_word(tl, 0, "$x \" \\ \\n");
    assert(tl->items[0].qmask[0] == Q_SINGLE);   /* escaped $: literal */
    assert(tl->items[0].qmask[1] == Q_DOUBLE);
    expect_word(tl, 1, "$y");
    assert(tl->items[1].qmask[0] == Q_NONE);
    token_list_free(tl);

    /* '$HOME'$HOME: only the second $ is expandable. */
    tl = lex("'$HOME'$HOME", NULL);
    assert(tl && tl->count == 1);
    assert(tl->items[0].qmask[0] == Q_SINGLE);
    assert(tl->items[0].qmask[5] == Q_NONE);
    token_list_free(tl);
}

static void test_misc(void)
{
    /* Empty quotes make an empty word; blanks and a trailing newline split. */
    TokenList *tl = lex("  echo \"\" ''\t x\n", NULL);
    assert(tl && tl->count == 4);
    expect_word(tl, 1, "");
    expect_word(tl, 2, "");
    expect_word(tl, 3, "x");
    token_list_free(tl);

    /* The slot after the last character records whether the word had quotes. */
    tl = lex("a \"\"b ''", NULL);
    assert(tl && tl->count == 3);
    assert(tl->items[0].qmask[1] == Q_NONE);
    assert(tl->items[1].qmask[1] == Q_SINGLE);
    assert(tl->items[2].qmask[0] == Q_SINGLE);
    token_list_free(tl);

    tl = lex("   \n", NULL);
    assert(tl && tl->count == 0);
    token_list_free(tl);

    /* Backslash-newline continues the word; a final backslash is literal. */
    tl = lex("ab\\\ncd e\\", NULL);
    assert(tl && tl->count == 2);
    expect_word(tl, 0, "abcd");
    expect_word(tl, 1, "e\\");
    token_list_free(tl);

    token_list_free(NULL);
}

/* SEC-001: tokens of any length (64 KiB) through dynamic buffers. */
static void test_long_token(void)
{
    size_t n = 64 * 1024;
    char *line = malloc(n + 3);
    assert(line);
    line[0] = '"';
    memset(line + 1, 'x', n);
    line[n + 1] = '"';
    line[n + 2] = '\0';

    TokenList *tl = lex(line, NULL);
    assert(tl && tl->count == 1 && strlen(tl->items[0].text) == n);
    token_list_free(tl);
    free(line);
}

/* The argv adapter used by the executor and alias expansion. */
static void test_argv_adapter(void)
{
    char **v;
    int n = lexer_tokenize("cat <in >>out 'x y' &", &v);

    assert(n == 7);
    assert(strcmp(v[0], "cat") == 0);
    assert(strcmp(v[1], "\x1f<") == 0);
    assert(strcmp(v[2], "in") == 0);
    assert(strcmp(v[3], "\x1f>>") == 0);
    assert(strcmp(v[4], "out") == 0);
    assert(strcmp(v[5], "x y") == 0);
    assert(strcmp(v[6], "\x1e&") == 0);
    assert(v[7] == NULL);
    lexer_free(v);

    n = lexer_tokenize("echo '>'", &v);
    assert(n == 2 && strcmp(v[1], ">") == 0);
    lexer_free(v);

    assert(lexer_tokenize("echo 'oops", &v) == -1);
}

int main(void)
{
    test_quotes_and_escapes();
    test_unterminated();
    test_operators_no_spaces();
    test_all_operators();
    test_double_quote_escapes();
    test_misc();
    test_long_token();
    test_argv_adapter();
    puts("test_lexer OK");
    return 0;
}
