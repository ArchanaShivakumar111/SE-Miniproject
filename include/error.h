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

#endif
