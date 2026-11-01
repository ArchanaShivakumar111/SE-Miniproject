#ifndef PIPELINE_H
#define PIPELINE_H

#include "shell.h"

/* FR-005: a pipeline is 1 to MAX_PIPELINE_STAGES commands joined by '|'. */
#define MAX_PIPELINE_STAGES 16

/* How the lexer spells an unquoted '|' in argv (see token_list_to_argv). */
#define PIPE_MARKER "\x1f|"

#define PIPELINE_NOMEM    (-1)
#define PIPELINE_TOO_MANY (-2)

typedef struct {
    char **argv;    /* NULL-terminated; owns its strings */
    int argc;
} PipeStage;

typedef struct {
    PipeStage stages[MAX_PIPELINE_STAGES];
    int count;
} Pipeline;

/* Splits a lexer argv at PIPE_MARKER words. Takes ownership of argv and
 * its strings in every case: on success they live in pl, on failure they
 * are freed. Returns 0, PIPELINE_NOMEM or PIPELINE_TOO_MANY. */
int pipeline_split(char **argv, int argc, Pipeline *pl);

/* Inverse of pipeline_split: one flat argv with PIPE_MARKER between the
 * stages. On success pl is emptied; on failure (-1) it is left unchanged. */
int pipeline_join(Pipeline *pl, char ***argv, int *argc);

void pipeline_free(Pipeline *pl);

/* For a pipeline of 2 or more stages: every stage must hold a command
 * once its redirections are removed. Prints the error and returns -1
 * otherwise. Nothing is run or opened. */
int pipeline_validate(const Pipeline *pl);

/* Starts every stage, connected by pipes, and waits for all of them.
 * Returns the exit status of the last stage ($?). Meant to run in a
 * forked process of its own (see shell.c), so it never changes the
 * shell's state: built-ins in a pipeline run in a child (SEC-005). */
int pipeline_run_stages(Shell *sh, const Pipeline *pl);

#endif
