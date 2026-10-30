/*
 * SEC-T01 / SEC-001 fuzzer. Deterministic (seeded xorshift), so failures
 * reproduce.
 *
 *   fuzz lex  [count] [seed]  run count random lines (up to 1 MB) through
 *                             lex() -> expand() -> token_list_to_argv()
 *   fuzz emit [count] [seed]  print count random lines for feeding the
 *                             shell itself; '/', '.', NUL and newline are
 *                             never emitted, so redirections and cd stay
 *                             inside the current directory
 *
 * Build with -fsanitize=address,undefined; any report fails the run.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expander.h"
#include "lexer.h"

#define MB (1024 * 1024)

static unsigned long long rng_state;

static unsigned long long rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static size_t pick_len(int i)
{
    if (i == 0)
        return MB;                          /* always one full 1 MB line */
    if (i % 200 == 1)
        return (size_t)(rng() % MB) + 1;    /* a few up to 1 MB */
    if (i % 10 == 0)
        return (size_t)(rng() % 8192);
    return (size_t)(rng() % 256);
}

/* Random bytes biased towards characters the lexer and expander treat
 * specially. safe = 1 leaves out '/' and '.' for the shell-level run. */
static void fill(char *buf, size_t len, int safe)
{
    static const char special[] = "'\"\\$?|<>&2 \t_=";
    static const char word[] = "abcdeHOMEXYZ019_";

    for (size_t i = 0; i < len; i++) {
        unsigned r = (unsigned)(rng() % 100);
        char c;

        if (r < 35)
            c = special[rng() % (sizeof special - 1)];
        else if (r < 75)
            c = word[rng() % (sizeof word - 1)];
        else
            c = (char)(rng() % 255 + 1);    /* any byte but NUL */

        if (c == '\n' || (safe && (c == '/' || c == '.')))
            c = ' ';
        buf[i] = c;
    }
    buf[len] = '\0';
}

static int run_lex(int count)
{
    static const char *values[] = {
        "", "a b", "$HOME", "'q'", "\"", "\\", "$(rm x)", "a > b | c &",
        "\x1f>", "x\x1e&", "$?$?",
    };
    Shell sh = {0};
    char *buf = malloc(MB + 1);
    if (!buf)
        return 1;

    for (int i = 0; i < count; i++) {
        size_t len = pick_len(i);
        fill(buf, len, 0);

        setenv("X", values[rng() % (sizeof values / sizeof values[0])], 1);
        setenv("HOME", values[rng() % (sizeof values / sizeof values[0])], 1);
        sh.last_status = (int)(rng() % 256);

        ShError err;
        TokenList *tl = lex(buf, &err);
        if (!tl) {
            if (err.code != ERR_SYNTAX) {
                fprintf(stderr, "fuzz: line %d: lex failed with code %d\n", i, err.code);
                free(buf);
                return 1;
            }
            continue;
        }
        if (expand(tl, &sh) != 0) {
            fprintf(stderr, "fuzz: line %d: expand failed\n", i);
            token_list_free(tl);
            free(buf);
            return 1;
        }
        char **argv;
        int n = token_list_to_argv(tl, &argv);
        if (n >= 0)
            lexer_free(argv);
        else if (n != LEXER_EMARKER) {
            fprintf(stderr, "fuzz: line %d: argv conversion failed\n", i);
            token_list_free(tl);
            free(buf);
            return 1;
        }
        token_list_free(tl);
    }
    free(buf);
    printf("fuzz lex: %d lines OK\n", count);
    return 0;
}

static int run_emit(int count)
{
    char *buf = malloc(MB + 1);
    if (!buf)
        return 1;
    for (int i = 0; i < count; i++) {
        size_t len = pick_len(i);
        fill(buf, len, 1);
        fwrite(buf, 1, len, stdout);
        fputc('\n', stdout);
    }
    free(buf);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "lex";
    int count = argc > 2 ? atoi(argv[2]) : 1000;
    rng_state = argc > 3 ? strtoull(argv[3], NULL, 0) : 0x5EC001ULL;
    if (rng_state == 0)
        rng_state = 1;

    if (strcmp(mode, "lex") == 0)
        return run_lex(count);
    if (strcmp(mode, "emit") == 0)
        return run_emit(count);
    fprintf(stderr, "usage: fuzz lex|emit [count] [seed]\n");
    return 2;
}
