#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "history.h"

int main(void)
{
    History *h = history_create();
    assert(h);
    history_add(h, "ls\n");
    history_add(h, "\n");
    assert(history_count(h) == 1 && strcmp(history_get(h, 0), "ls") == 0);

    for (int i = 0; i < 1500; i++) {
        char buf[32];
        snprintf(buf, sizeof buf, "cmd%d", i);
        history_add(h, buf);
    }
    assert(history_count(h) == HISTORY_MAX);
    assert(strcmp(history_get(h, 0), "cmd500") == 0);
    assert(strcmp(history_get(h, HISTORY_MAX - 1), "cmd1499") == 0);
    assert(history_get(h, HISTORY_MAX) == NULL);

    char path[] = "/tmp/hist_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    assert(history_save(h, path) == 0);

    History *h2 = history_create();
    assert(history_load(h2, path) == 0);
    assert(history_count(h2) == HISTORY_MAX);
    assert(strcmp(history_get(h2, 0), "cmd500") == 0);
    unlink(path);

    assert(history_load(h2, "/no/such/file") == 0);
    history_destroy(h);
    history_destroy(h2);
    puts("test_history OK");
    return 0;
}
