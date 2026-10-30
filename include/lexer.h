#ifndef LEXER_H
#define LEXER_H

/* Splits line into a NULL-terminated argv honouring quotes and escapes.
 * Returns the token count, or -1 on error (unterminated quote, OOM). */
int lexer_tokenize(const char *line, char ***argv_out);
void lexer_free(char **argv);

#endif
