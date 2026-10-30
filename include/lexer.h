#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

/* FR-002: lexer with quotes, escapes and operators. Owner: lexer author. */
typedef enum {
    TOK_WORD,
    TOK_PIPE,        /* |   */
    TOK_REDIR_IN,    /* <   */
    TOK_REDIR_OUT,   /* >   */
    TOK_REDIR_APPEND,/* >>  */
    TOK_REDIR_ERR,   /* 2>  */
    TOK_REDIR_BOTH,  /* &>  */
    TOK_BACKGROUND,  /* &   */
    TOK_END
} TokenType;

typedef struct {
    TokenType type;
    char     *text;  /* heap-allocated for TOK_WORD, NULL otherwise */
} Token;

/* Returns number of tokens (>= 0) or -1 on syntax error. Caller frees. */
int  lexer_tokenize(const char *line, Token **out_tokens);
void lexer_free(Token *tokens, int count);

#endif /* LEXER_H */
