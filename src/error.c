#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PREFIX "simple_shell: "

/* Writes all of buf, retrying short writes and EINTR. */
static void write_all(const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(STDERR_FILENO, buf, len);
        if (n <= 0)
            return;
        buf += n;
        len -= (size_t)n;
    }
}

void sh_error(const char *ctx, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    int body = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (body < 0)
        body = 0;

    size_t ctx_len = ctx ? strlen(ctx) + 2 : 0;
    size_t len = strlen(PREFIX) + ctx_len + (size_t)body + 1;
    char *msg = malloc(len + 1);

    if (!msg) {
        /* Out of memory: still say something, unformatted. */
        write_all(PREFIX "out of memory\n", strlen(PREFIX "out of memory\n"));
        return;
    }

    size_t off = (size_t)snprintf(msg, len + 1, PREFIX "%s%s",
                                  ctx ? ctx : "", ctx ? ": " : "");
    va_start(ap, fmt);
    vsnprintf(msg + off, len + 1 - off, fmt, ap);
    va_end(ap);
    msg[len - 1] = '\n';
    msg[len] = '\0';

    write_all(msg, len);
    free(msg);
}
