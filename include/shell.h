#ifndef SHELL_H
#define SHELL_H

struct History;
struct AliasTable;

typedef struct Shell {
    struct History *history;
    struct AliasTable *aliases;
    int last_status;
    int running;
} Shell;

int shell_execute_line(Shell *sh, const char *line);

#endif
