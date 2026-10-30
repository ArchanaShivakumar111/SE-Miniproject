#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

#include "error.h"

/* SAD 9.1 */
typedef enum {
    T_WORD,
    T_PIPE,         /* |  */
    T_LESS,         /* <  */
    T_GREAT,        /* >  */
    T_DGREAT,       /* >> */
    T_ERR_GREAT,    /* 2> */
    T_AND_GREAT,    /* &> */
    T_AMP           /* &  */
} TokenType;

/* qmask values, one per character of a WORD's text. */
#define Q_NONE   0  /* unquoted: subject to expansion */
#define Q_SINGLE 1  /* single-quoted or backslash-escaped: always literal */
#define Q_DOUBLE 2  /* double-quoted: $ still expanded, no word splitting */

typedef struct {
    TokenType type;
    char *text;             /* WORD text, or the operator spelling */
    unsigned char *qmask;   /* strlen(text) entries for WORDs, NULL for operators */
} Token;

typedef struct {
    Token *items;
    size_t count, cap;
} TokenList;

/* Splits line into tokens (SAD 9.2). Returns NULL and fills err with
 * ERR_SYNTAX (unterminated quote) or ERR_NOMEM on failure. line is not
 * modified. */
TokenList *lex(const char *line, ShError *err);
void token_list_free(TokenList *tl);

/* Flattens tokens into a NULL-terminated argv. Operators are encoded as
 * "\x1f" + spelling ("\x1e&" for &) so redirection_extract() and the
 * executor can tell them from quoted words. T_PIPE is emitted as a plain
 * "|" word, as before, until the executor handles pipelines again
 * (FR-005). Returns argc or -1 (OOM). */
int token_list_to_argv(const TokenList *tl, char ***argv_out);

/* lex() + token_list_to_argv(). Returns the token count, or -1 on error
 * (unterminated quote, OOM). */
int lexer_tokenize(const char *line, char ***argv_out);
void lexer_free(char **argv);

#endif
