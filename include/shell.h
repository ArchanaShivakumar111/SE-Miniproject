#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>

struct History;
struct AliasTable;
struct JobTable;

typedef struct Shell {
    struct History *history;
    struct AliasTable *aliases;
    struct JobTable *jobs;
    int last_status;
    int running;
    int interactive;
    pid_t shell_pgid;
} Shell;

int shell_execute_line(Shell *sh, const char *line);

#endif
