#include "repl.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int is_set(const char *s)
{
    return s && *s;
}

char *prompt_format(const char *user, const char *host,
                    const char *cwd, const char *home)
{
    if (!is_set(user) || !is_set(host) || !is_set(cwd))
        return strdup("$ ");

    const char *tail = cwd;
    int tilde = 0;

    if (is_set(home)) {
        size_t n = strlen(home);

        while (n > 1 && home[n - 1] == '/')
            n--;
        if (strncmp(cwd, home, n) == 0 &&
            (cwd[n] == '\0' || (cwd[n] == '/' && n > 1))) {
            tail = cwd + n;
            tilde = 1;
        }
    }

    size_t len = strlen(user) + strlen(host) + strlen(tail) + 6;
    char *out = malloc(len);

    if (!out)
        return NULL;
    snprintf(out, len, "%s@%s:%s%s$ ", user, host, tilde ? "~" : "", tail);
    return out;
}

/* getcwd() into a buffer that grows until the path fits. */
static char *current_dir(void)
{
    size_t size = 256;
    char *buf = NULL;

    for (;;) {
        char *nb = realloc(buf, size);
        if (!nb) {
            free(buf);
            return NULL;
        }
        buf = nb;
        if (getcwd(buf, size))
            return buf;
        if (errno != ERANGE) {
            free(buf);
            return NULL;
        }
        size *= 2;
    }
}

void prompt_render(const Shell *sh)
{
    (void)sh;
    const struct passwd *pw = getpwuid(getuid());
    char host[256];
    const char *hostp = NULL;

    if (gethostname(host, sizeof(host)) == 0) {
        host[sizeof(host) - 1] = '\0';
        hostp = host;
    }

    const char *home = getenv("HOME");
    if (!home && pw)
        home = pw->pw_dir;

    char *cwd = current_dir();
    char *prompt = prompt_format(pw ? pw->pw_name : NULL, hostp, cwd, home);

    fputs(prompt ? prompt : "$ ", stdout);
    fflush(stdout);
    free(prompt);
    free(cwd);
}

int line_is_blank(const char *line)
{
    for (; *line; line++) {
        if (*line != ' ' && *line != '\t' &&
            *line != '\n' && *line != '\r')
            return 0;
    }
    return 1;
}
