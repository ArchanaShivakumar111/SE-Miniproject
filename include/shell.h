#ifndef SHELL_H
#define SHELL_H

#include <limits.h>
#include <stdbool.h>
#include <sys/types.h>

#define SHELL_NAME            "simple_shell"
#define MAX_LINE              4096
#define MAX_PIPELINE_STAGES   16
#define MAX_JOBS              64
#define HISTORY_SIZE          1000
#define NAME_BUF              256

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Forward declarations: each owning module defines its own struct. */
struct History;
struct JobTable;

/*
 * ShellContext: all mutable shell state lives here and is passed to every
 * module. No global variables, so modules stay independent and testable.
 */
typedef struct ShellContext {
    int              last_status;   /* value of $?                         */
    bool             interactive;   /* true if stdin is a terminal         */
    bool             running;       /* REPL loop continues while true      */
    int              exit_code;     /* status returned by main()           */

    char             username[NAME_BUF];
    char             hostname[NAME_BUF];
    char             cwd[PATH_MAX];

    pid_t            shell_pgid;    /* shell's own process group           */
    struct History  *history;       /* owned by history module (FR-010)    */
    struct JobTable *jobs;          /* owned by jobs module (FR-007)       */
} ShellContext;

/* shell.c */
int  ctx_init(ShellContext *ctx);
void ctx_destroy(ShellContext *ctx);
void ctx_refresh_cwd(ShellContext *ctx);

/* NFR-007: all errors go to stderr prefixed with "simple_shell: " */
void shell_error(const char *fmt, ...);

#endif /* SHELL_H */
