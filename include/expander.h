#ifndef EXPANDER_H
#define EXPANDER_H

#include "lexer.h"
#include "shell.h"

/*
 * FR-003 (SAD 10.2): replaces $NAME with the environment value (empty if
 * unset) and $? with sh->last_status in every WORD, except where the $ is
 * single-quoted or escaped (qmask Q_SINGLE). Substituted text is inserted
 * as literal characters: it is never re-lexed or split into more words
 * (SEC-002). A word that was entirely unquoted and expands to nothing
 * ($UNDEF) is removed; "$UNDEF" stays as an empty argument.
 * Returns 0, or 1 on allocation failure (tl is then unchanged).
 */
int expand(TokenList *tl, const Shell *sh);

#endif
