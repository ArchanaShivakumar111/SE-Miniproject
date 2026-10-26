#include "lexer.h"

#include <stdlib.h>
#include <string.h>

void lexer_free(char **argv)
{
    if (!argv)
        return;
    for (char **p = argv; *p; p++)
        free(*p);
    free(argv);
}

static int push_token(char ***v, size_t *n, size_t *cap, char *tok)
{
    if (*n + 2 > *cap) {
        size_t ncap = *cap ? *cap * 2 : 8;
        char **nv = realloc(*v, ncap * sizeof(char *));
        if (!nv)
            return -1;
        *v = nv;
        *cap = ncap;
    }
    (*v)[(*n)++] = tok;
    (*v)[*n] = NULL;
    return 0;
}

int lexer_tokenize(const char *line, char ***argv_out)
{
    char **v = NULL;
    size_t n = 0, cap = 0;
    size_t len = strlen(line);
    char *buf = malloc(len + 1);
    if (!buf)
        return -1;

    size_t b = 0;
    int in_tok = 0, sq = 0, dq = 0;

    for (size_t i = 0; line[i]; i++) {
        char c = line[i];
        if (sq) {
            if (c == '\'') sq = 0; else buf[b++] = c;
        } else if (dq) {
            if (c == '"') dq = 0;
            else if (c == '\\' && line[i + 1] &&
                     strchr("\"\\$", line[i + 1])) buf[b++] = line[++i];
            else buf[b++] = c;
        } else if (c == '\\') {
            if (line[i + 1]) buf[b++] = line[++i];
            in_tok = 1;
        } else if (c == '\'') {
            sq = 1; in_tok = 1;
        } else if (c == '"') {
            dq = 1; in_tok = 1;
        } else if (c == ' ' || c == '\t' || c == '\n') {
            if (in_tok) {
                buf[b] = '\0';
                char *t = strdup(buf);
                if (!t || push_token(&v, &n, &cap, t) < 0) { free(t); goto fail; }
                b = 0; in_tok = 0;
            }
            continue;
        } else {
            buf[b++] = c;
            in_tok = 1;
        }
        in_tok = 1;
    }
    if (sq || dq)
        goto fail;
    if (in_tok) {
        buf[b] = '\0';
        char *t = strdup(buf);
        if (!t || push_token(&v, &n, &cap, t) < 0) { free(t); goto fail; }
    }
    if (!v) {
        v = malloc(sizeof(char *));
        if (!v)
            goto fail;
        v[0] = NULL;
    }
    free(buf);
    *argv_out = v;
    return (int)n;

fail:
    free(buf);
    lexer_free(v);
    return -1;
}
