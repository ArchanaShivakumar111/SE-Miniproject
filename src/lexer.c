#include "lexer.h"

#include <stdlib.h>
#include <string.h>

/* Growable text + qmask buffer for the word being scanned (SEC-001). */
typedef struct {
    char *text;
    unsigned char *qmask;
    size_t len, cap;
    int quoted;     /* the word contained quote characters */
} WordBuf;

static int word_put(WordBuf *w, char c, unsigned char q)
{
    if (w->len + 1 >= w->cap) {
        size_t ncap = w->cap ? w->cap * 2 : 32;
        char *nt = realloc(w->text, ncap);
        if (!nt)
            return -1;
        w->text = nt;
        unsigned char *nq = realloc(w->qmask, ncap);
        if (!nq)
            return -1;
        w->qmask = nq;
        w->cap = ncap;
    }
    w->text[w->len] = c;
    w->qmask[w->len] = q;
    w->len++;
    return 0;
}

static int list_push(TokenList *tl, TokenType type,
                     char *text, unsigned char *qmask)
{
    if (tl->count == tl->cap) {
        size_t ncap = tl->cap ? tl->cap * 2 : 8;
        Token *ni = realloc(tl->items, ncap * sizeof(*ni));
        if (!ni)
            return -1;
        tl->items = ni;
        tl->cap = ncap;
    }
    tl->items[tl->count].type = type;
    tl->items[tl->count].text = text;
    tl->items[tl->count].qmask = qmask;
    tl->count++;
    return 0;
}

/* Moves the scanned word into the list as a WORD token. */
static int emit_word(TokenList *tl, WordBuf *w)
{
    char *text = malloc(w->len + 1);
    unsigned char *qmask = malloc(w->len + 1);

    if (!text || !qmask || list_push(tl, T_WORD, text, qmask) < 0) {
        free(text);
        free(qmask);
        return -1;
    }
    if (w->len) {
        memcpy(text, w->text, w->len);
        memcpy(qmask, w->qmask, w->len);
    }
    text[w->len] = '\0';
    qmask[w->len] = w->quoted ? Q_SINGLE : Q_NONE;
    w->len = 0;
    w->quoted = 0;
    return 0;
}

/* Recognises an operator at s. at_word_start is 1 when no word is being
 * scanned, so that "a2>f" stays the word "a2" followed by >. */
static size_t match_operator(const char *s, int at_word_start, TokenType *type)
{
    if (s[0] == '|') {
        *type = T_PIPE;
        return 1;
    }
    if (s[0] == '<') {
        *type = T_LESS;
        return 1;
    }
    if (s[0] == '>') {
        if (s[1] == '>') {
            *type = T_DGREAT;
            return 2;
        }
        *type = T_GREAT;
        return 1;
    }
    if (s[0] == '&') {
        if (s[1] == '>') {
            *type = T_AND_GREAT;
            return 2;
        }
        *type = T_AMP;
        return 1;
    }
    if (s[0] == '2' && s[1] == '>' && at_word_start) {
        *type = T_ERR_GREAT;
        return 2;
    }
    return 0;
}

static int is_blank_char(char c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

void token_list_free(TokenList *tl)
{
    if (!tl)
        return;
    for (size_t i = 0; i < tl->count; i++) {
        free(tl->items[i].text);
        free(tl->items[i].qmask);
    }
    free(tl->items);
    free(tl);
}

static void set_err(ShError *err, ErrCode code, const char *msg)
{
    if (err) {
        err->code = code;
        err->msg = msg;
    }
}

/*
 * Single-pass DFA (SAD 9.2): START/WORD outside quotes, IN_SQUOTE,
 * IN_DQUOTE; escapes are handled inline. A token boundary is emitted only
 * outside quotes.
 */
TokenList *lex(const char *line, ShError *err)
{
    enum { START, WORD, IN_SQUOTE, IN_DQUOTE } state = START;
    TokenList *tl = calloc(1, sizeof(*tl));
    WordBuf w = {0};

    set_err(err, ERR_NONE, NULL);
    if (!tl)
        goto nomem;

    for (size_t i = 0; line[i]; i++) {
        char c = line[i];

        if (state == IN_SQUOTE) {
            if (c == '\'')
                state = WORD;
            else if (word_put(&w, c, Q_SINGLE) < 0)
                goto nomem;
            continue;
        }

        if (state == IN_DQUOTE) {
            if (c == '"') {
                state = WORD;
            } else if (c == '\\' && line[i + 1] &&
                       strchr("$\"\\", line[i + 1])) {
                /* An escaped $ must not be expanded later. */
                if (word_put(&w, line[++i], Q_SINGLE) < 0)
                    goto nomem;
            } else if (word_put(&w, c, Q_DOUBLE) < 0) {
                goto nomem;
            }
            continue;
        }

        /* START or WORD: outside quotes. */
        if (c == '\\') {
            if (line[i + 1] == '\n') {
                i++;                /* line continuation: drop both */
                continue;
            }
            if (line[i + 1]) {
                if (word_put(&w, line[++i], Q_SINGLE) < 0)
                    goto nomem;
            } else if (word_put(&w, c, Q_SINGLE) < 0) {
                goto nomem;         /* trailing backslash is literal */
            }
            state = WORD;
            continue;
        }
        if (c == '\'') {
            state = IN_SQUOTE;
            w.quoted = 1;
            continue;
        }
        if (c == '"') {
            state = IN_DQUOTE;
            w.quoted = 1;
            continue;
        }

        TokenType op;
        size_t op_len = match_operator(line + i, state == START, &op);

        if (op_len || is_blank_char(c)) {
            if (state == WORD && emit_word(tl, &w) < 0)
                goto nomem;
            state = START;
            if (op_len) {
                char *text = malloc(op_len + 1);
                if (!text)
                    goto nomem;
                memcpy(text, line + i, op_len);
                text[op_len] = '\0';
                if (list_push(tl, op, text, NULL) < 0) {
                    free(text);
                    goto nomem;
                }
                i += op_len - 1;
            }
            continue;
        }

        if (word_put(&w, c, Q_NONE) < 0)
            goto nomem;
        state = WORD;
    }

    if (state == IN_SQUOTE || state == IN_DQUOTE) {
        set_err(err, ERR_SYNTAX, "unterminated quote");
        goto fail;
    }
    if (state == WORD && emit_word(tl, &w) < 0)
        goto nomem;

    free(w.text);
    free(w.qmask);
    return tl;

nomem:
    set_err(err, ERR_NOMEM, "out of memory");
fail:
    free(w.text);
    free(w.qmask);
    token_list_free(tl);
    return NULL;
}

void lexer_free(char **argv)
{
    if (!argv)
        return;
    for (char **p = argv; *p; p++)
        free(*p);
    free(argv);
}

int token_list_to_argv(const TokenList *tl, char ***argv_out)
{
    char **v = calloc(tl->count + 1, sizeof(*v));
    if (!v)
        return -1;

    for (size_t i = 0; i < tl->count; i++) {
        const Token *t = &tl->items[i];

        /* A word starting with a marker byte would be taken for an
         * operator downstream (e.g. '\x1f>' as a redirect): refuse it. */
        if (t->type == T_WORD &&
            (t->text[0] == '\x1e' || t->text[0] == '\x1f')) {
            lexer_free(v);
            return LEXER_EMARKER;
        }

        if (t->type == T_WORD || t->type == T_PIPE) {
            v[i] = strdup(t->text);
        } else {
            size_t n = strlen(t->text);
            v[i] = malloc(n + 2);
            if (v[i]) {
                v[i][0] = t->type == T_AMP ? '\x1e' : '\x1f';
                memcpy(v[i] + 1, t->text, n + 1);
            }
        }
        if (!v[i]) {
            lexer_free(v);
            return -1;
        }
    }
    *argv_out = v;
    return (int)tl->count;
}

int lexer_tokenize(const char *line, char ***argv_out)
{
    TokenList *tl = lex(line, NULL);
    if (!tl)
        return -1;

    int n = token_list_to_argv(tl, argv_out);
    token_list_free(tl);
    return n;
}
