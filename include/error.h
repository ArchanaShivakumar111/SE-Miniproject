#ifndef ERROR_H
#define ERROR_H

/* Error codes filled in by APIs that return NULL on failure (SAD 10.1). */
typedef enum {
    ERR_NONE = 0,
    ERR_SYNTAX,
    ERR_NOMEM
} ErrCode;

typedef struct {
    ErrCode code;
    const char *msg;    /* static string, never freed */
} ShError;

/* NFR-007: the one place shell errors are printed. Writes
 * "simple_shell: <ctx>: <message>\n" to stderr in a single write
 * ("simple_shell: <message>" if ctx is NULL). Nothing goes to stdout. */
void sh_error(const char *ctx, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/* Exit statuses (SAD 11.2). */
#define ST_GENERAL      1   /* general, redirection or out-of-memory error */
#define ST_SYNTAX       2   /* syntax or usage error */
#define ST_NOEXEC     126   /* found but not executable */
#define ST_NOTFOUND   127   /* command not found */
#define ST_SIGNAL_BASE 128  /* + n: killed by signal n */

#endif
