#ifndef REDIRECTION_H
#define REDIRECTION_H

#include <stddef.h>

typedef enum {
    REDIR_INPUT,
    REDIR_OUTPUT,
    REDIR_APPEND,
    REDIR_ERROR,
    REDIR_BOTH
} RedirectionKind;

typedef struct {
    RedirectionKind kind;
    char *path;
} RedirectionAction;

typedef struct {
    RedirectionAction *actions;
    size_t count;
} RedirectionList;

int redirection_extract(
    char **argv,
    int argc,
    char ***command_argv,
    int *command_argc,
    RedirectionList *list
);

int redirection_apply(
    const RedirectionList *list,
    int saved_fds[3]
);

void redirection_restore(int saved_fds[3]);
void redirection_free(RedirectionList *list);

#endif
