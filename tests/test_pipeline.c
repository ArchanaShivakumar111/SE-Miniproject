/* FR-005: unit tests for splitting a lexer argv into pipeline stages. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline.h"

/* Builds a heap argv the way token_list_to_argv does (NULL-terminated,
 * every string malloc'd). A "|" argument becomes the pipe marker. */
static char **make_argv(const char *const *words, int n)
{
    char **v = calloc((size_t)n + 1, sizeof *v);

    assert(v);
    for (int i = 0; i < n; i++) {
        v[i] = strdup(strcmp(words[i], "|") == 0 ? PIPE_MARKER : words[i]);
        assert(v[i]);
    }
    return v;
}

static void test_single_command(void)
{
    const char *w[] = {"ls", "-l"};
    Pipeline pl;

    assert(pipeline_split(make_argv(w, 2), 2, &pl) == 0);
    assert(pl.count == 1);
    assert(pl.stages[0].argc == 2);
    assert(strcmp(pl.stages[0].argv[1], "-l") == 0);
    assert(pl.stages[0].argv[2] == NULL);
    pipeline_free(&pl);
}

static void test_three_stages(void)
{
    const char *w[] = {"a", "x", "|", "b", "|", "c", "y", "z"};
    Pipeline pl;

    assert(pipeline_split(make_argv(w, 8), 8, &pl) == 0);
    assert(pl.count == 3);
    assert(pl.stages[0].argc == 2);
    assert(pl.stages[1].argc == 1);
    assert(pl.stages[2].argc == 3);
    assert(strcmp(pl.stages[1].argv[0], "b") == 0);
    assert(strcmp(pl.stages[2].argv[2], "z") == 0);
    pipeline_free(&pl);
}

static void test_empty_stages_are_kept(void)
{
    const char *w[] = {"|", "ls", "|"};
    Pipeline pl;

    assert(pipeline_split(make_argv(w, 3), 3, &pl) == 0);
    assert(pl.count == 3);
    assert(pl.stages[0].argc == 0 && pl.stages[2].argc == 0);
    assert(pl.stages[1].argc == 1);
    assert(pipeline_validate(&pl) == -1);   /* an empty stage is an error */
    pipeline_free(&pl);
}

static void test_limit(void)
{
    const char *w[2 * MAX_PIPELINE_STAGES];
    Pipeline pl;
    int n = 0;

    /* MAX_PIPELINE_STAGES commands: allowed. */
    for (int i = 0; i < MAX_PIPELINE_STAGES; i++) {
        if (i > 0)
            w[n++] = "|";
        w[n++] = "cat";
    }
    assert(pipeline_split(make_argv(w, n), n, &pl) == 0);
    assert(pl.count == MAX_PIPELINE_STAGES);
    pipeline_free(&pl);

    /* One more: refused, and nothing leaks (checked under ASan). */
    const char *w2[2 * MAX_PIPELINE_STAGES + 2];

    n = 0;
    for (int i = 0; i < MAX_PIPELINE_STAGES + 1; i++) {
        if (i > 0)
            w2[n++] = "|";
        w2[n++] = "cat";
    }
    assert(pipeline_split(make_argv(w2, n), n, &pl) == PIPELINE_TOO_MANY);
    assert(pl.count == 0);
}

static void test_join_round_trip(void)
{
    const char *w[] = {"a", "|", "b", "c", "|", "d"};
    Pipeline pl;
    char **flat = NULL;
    int flat_argc = 0;

    assert(pipeline_split(make_argv(w, 6), 6, &pl) == 0);
    assert(pipeline_join(&pl, &flat, &flat_argc) == 0);
    assert(pl.count == 0);
    assert(flat_argc == 6);
    assert(strcmp(flat[0], "a") == 0);
    assert(strcmp(flat[1], PIPE_MARKER) == 0);
    assert(strcmp(flat[4], PIPE_MARKER) == 0);
    assert(strcmp(flat[5], "d") == 0 && flat[6] == NULL);

    assert(pipeline_split(flat, flat_argc, &pl) == 0);
    assert(pl.count == 3 && pl.stages[1].argc == 2);
    pipeline_free(&pl);
}

static void test_validate(void)
{
    const char *good[] = {"ls", "|", "wc"};
    const char *redir_only[] = {"\x1f>" , "f", "|", "cat"};
    Pipeline pl;

    assert(pipeline_split(make_argv(good, 3), 3, &pl) == 0);
    assert(pipeline_validate(&pl) == 0);
    pipeline_free(&pl);

    /* "> f | cat": a redirection but no command in the first stage. */
    char **v = make_argv(redir_only, 4);
    free(v[0]);
    v[0] = strdup("\x1f>");
    assert(pipeline_split(v, 4, &pl) == 0);
    assert(pipeline_validate(&pl) == -1);
    pipeline_free(&pl);
}

int main(void)
{
    test_single_command();
    test_three_stages();
    test_empty_stages_are_kept();
    test_limit();
    test_join_round_trip();
    test_validate();
    puts("test_pipeline OK");
    return 0;
}
