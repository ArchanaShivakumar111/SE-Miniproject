#include "redirection.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int operator_kind(const char *token, RedirectionKind *kind)
{
    if (strcmp(token, "\x1f<") == 0)
        *kind = REDIR_INPUT;
    else if (strcmp(token, "\x1f>") == 0)
        *kind = REDIR_OUTPUT;
    else if (strcmp(token, "\x1f>>") == 0)
        *kind = REDIR_APPEND;
    else if (strcmp(token, "\x1f" "2>") == 0)
        *kind = REDIR_ERROR;
    else if (strcmp(token, "\x1f&>") == 0)
        *kind = REDIR_BOTH;
    else
        return 0;

    return 1;
}

int redirection_extract(
    char **argv,
    int argc,
    char ***command_argv,
    int *command_argc,
    RedirectionList *list)
{
    if (!argv || argc < 0 || !command_argv ||
        !command_argc || !list)
        return -1;

    *command_argv = NULL;
    *command_argc = 0;
    list->actions = NULL;
    list->count = 0;

    char **command = calloc((size_t)argc + 1, sizeof(*command));
    RedirectionAction *actions =
        calloc((size_t)argc + 1, sizeof(*actions));

    if (!command || !actions) {
        free(command);
        free(actions);
        return -1;
    }

    int output_count = 0;

    for (int i = 0; i < argc; i++) {
        RedirectionKind kind;

        if (!operator_kind(argv[i], &kind)) {
            command[output_count++] = argv[i];
            continue;
        }

        if (i + 1 >= argc) {
            free(command);
            free(actions);
            return -1;
        }

        RedirectionKind next_kind;
        if (operator_kind(argv[i + 1], &next_kind)) {
            free(command);
            free(actions);
            return -1;
        }

        actions[list->count].kind = kind;
        actions[list->count].path = argv[++i];
        list->count++;
    }

    command[output_count] = NULL;
    list->actions = actions;
    *command_argv = command;
    *command_argc = output_count;
    return 0;
}

static int open_redirect_file(const char *path, int flags)
{
    int fd = open(path, flags, 0666);
    if (fd < 0)
        return -1;

    if (fd >= 3)
        return fd;

    int copy = fcntl(fd, F_DUPFD, 3);
    int saved_errno = errno;
    close(fd);

    if (copy < 0) {
        errno = saved_errno;
        return -1;
    }

    return copy;
}

void redirection_restore(int saved_fds[3])
{
    if (!saved_fds)
        return;

    for (int i = 0; i < 3; i++) {
        if (saved_fds[i] >= 0) {
            if (dup2(saved_fds[i], i) < 0)
                fprintf(stderr, "simple_shell: restore: %s\n",
                        strerror(errno));

            close(saved_fds[i]);
            saved_fds[i] = -1;
        }
    }
}

int redirection_apply(const RedirectionList *list, int saved_fds[3])
{
    if (!list || !saved_fds)
        return -1;

    for (int i = 0; i < 3; i++)
        saved_fds[i] = -1;

    if (list->count == 0)
        return 0;

    for (int i = 0; i < 3; i++) {
        saved_fds[i] = dup(i);
        if (saved_fds[i] < 0) {
            int error = errno;
            for (int j = 0; j < i; j++) {
                close(saved_fds[j]);
                saved_fds[j] = -1;
            }
            errno = error;
            fprintf(stderr, "simple_shell: dup: %s\n",
                    strerror(errno));
            return -1;
        }

        if (fcntl(saved_fds[i], F_SETFD, FD_CLOEXEC) < 0) {
            int error = errno;
            redirection_restore(saved_fds);
            errno = error;
            fprintf(stderr, "simple_shell: fcntl: %s\n",
                    strerror(errno));
            return -1;
        }
    }

    for (size_t i = 0; i < list->count; i++) {
        const RedirectionAction *action = &list->actions[i];
        int flags;
        int target;

        switch (action->kind) {
        case REDIR_INPUT:
            flags = O_RDONLY;
            target = STDIN_FILENO;
            break;
        case REDIR_OUTPUT:
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            target = STDOUT_FILENO;
            break;
        case REDIR_APPEND:
            flags = O_WRONLY | O_CREAT | O_APPEND;
            target = STDOUT_FILENO;
            break;
        case REDIR_ERROR:
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            target = STDERR_FILENO;
            break;
        case REDIR_BOTH:
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            target = STDOUT_FILENO;
            break;
        default:
            redirection_restore(saved_fds);
            return -1;
        }

        int fd = open_redirect_file(action->path, flags);
        if (fd < 0) {
            int error = errno;
            redirection_restore(saved_fds);
            errno = error;
            fprintf(stderr, "simple_shell: %s: %s\n",
                    action->path, strerror(errno));
            return -1;
        }

        if (dup2(fd, target) < 0 ||
            (action->kind == REDIR_BOTH &&
             dup2(fd, STDERR_FILENO) < 0)) {
            int error = errno;
            close(fd);
            redirection_restore(saved_fds);
            errno = error;
            fprintf(stderr, "simple_shell: dup2: %s\n",
                    strerror(errno));
            return -1;
        }

        close(fd);
    }

    return 0;
}

void redirection_free(RedirectionList *list)
{
    if (!list)
        return;

    free(list->actions);
    list->actions = NULL;
    list->count = 0;
}
