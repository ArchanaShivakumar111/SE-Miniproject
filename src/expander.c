#include "expander.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *text;
    unsigned char *qmask;
    size_t len, cap;
} Buf;

static int buf_put(Buf *b, const char *s, size_t n, unsigned char q)
{
    if (b->len + n + 1 > b->cap) {
        size_t ncap = b->cap ? b->cap : 32;
        while (ncap < b->len + n + 1)
            ncap *= 2;
        char *nt = realloc(b->text, ncap);
        if (!nt)
            return -1;
        b->text = nt;
        unsigned char *nq = realloc(b->qmask, ncap);
        if (!nq)
            return -1;
        b->qmask = nq;
        b->cap = ncap;
    }
    memcpy(b->text + b->len, s, n);
    memset(b->qmask + b->len, q, n);
    b->len += n;
    b->text[b->len] = '\0';
    b->qmask[b->len] = 0;
    return 0;
}

static int name_start(char c)
{
    return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int name_char(char c)
{
    return name_start(c) || (c >= '0' && c <= '9');
}

/* Expands one WORD into b. Sets *drop if the word should vanish. */
static int expand_word(const Token *t, const Shell *sh, Buf *b, int *drop)
{
    const char *s = t->text;
    const unsigned char *q = t->qmask;
    int quoted = 0, expanded = 0;

    for (size_t i = 0; s[i]; i++) {
        if (q[i] != Q_NONE)
            quoted = 1;

        /* A $ expands only if it and the next character are in the same
         * quoting context and not single-quoted/escaped. */
        if (s[i] != '$' || q[i] == Q_SINGLE || !s[i + 1] || q[i + 1] != q[i]) {
            if (buf_put(b, s + i, 1, q[i]) < 0)
                return -1;
            continue;
        }

        if (s[i + 1] == '?') {
            char num[16];
            int n = snprintf(num, sizeof num, "%d", sh ? sh->last_status : 0);
            if (buf_put(b, num, (size_t)n, Q_SINGLE) < 0)
                return -1;
            i++;
            expanded = 1;
            continue;
        }

        if (!name_start(s[i + 1])) {
            if (buf_put(b, "$", 1, q[i]) < 0)   /* "$", "$1", "$-": literal */
                return -1;
            continue;
        }

        size_t start = i + 1, end = start;
        while (s[end] && name_char(s[end]) && q[end] == q[i])
            end++;

        char *name = malloc(end - start + 1);
        if (!name)
            return -1;
        memcpy(name, s + start, end - start);
        name[end - start] = '\0';
        const char *value = getenv(name);
        free(name);

        /* Inserted as literal text: never re-expanded or re-lexed. */
        if (value && buf_put(b, value, strlen(value), Q_SINGLE) < 0)
            return -1;
        i = end - 1;
        expanded = 1;
    }

    if (q[strlen(s)] != Q_NONE)
        quoted = 1;     /* had quotes, e.g. ''$UNDEF */
    *drop = expanded && !quoted && b->len == 0;
    if (!b->text && buf_put(b, "", 0, Q_NONE) < 0)
        return -1;
    b->qmask[b->len] = quoted ? Q_SINGLE : Q_NONE;    /* lexer.h sentinel */
    return 0;
}

enum { KEEP, REPLACE, DROP };

int expand(TokenList *tl, const Shell *sh)
{
    size_t n = tl->count;
    if (n == 0)
        return 0;

    /* Phase 1: build every new word aside; on failure tl is untouched. */
    Buf *repl = calloc(n, sizeof(*repl));
    unsigned char *action = calloc(n, 1);
    if (!repl || !action) {
        free(repl);
        free(action);
        return 1;
    }

    for (size_t i = 0; i < n; i++) {
        const Token *t = &tl->items[i];
        int drop = 0;

        if (t->type != T_WORD || !strchr(t->text, '$'))
            continue;               /* action[i] = KEEP */

        if (expand_word(t, sh, &repl[i], &drop) < 0) {
            for (size_t j = 0; j <= i; j++) {
                free(repl[j].text);
                free(repl[j].qmask);
            }
            free(repl);
            free(action);
            return 1;
        }
        action[i] = drop ? DROP : REPLACE;
    }

    /* Phase 2 (cannot fail): swap in new words, compacting in place. */
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        Token *t = &tl->items[i];

        if (action[i] != KEEP) {
            free(t->text);
            free(t->qmask);
        }
        if (action[i] == DROP) {
            free(repl[i].text);
            free(repl[i].qmask);
            continue;
        }
        if (action[i] == REPLACE) {
            t->text = repl[i].text;
            t->qmask = repl[i].qmask;
        }
        tl->items[k++] = *t;
    }
    tl->count = k;

    free(repl);
    free(action);
    return 0;
}
