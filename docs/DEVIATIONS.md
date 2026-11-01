# Deviations from SRS v1.1 / SAD v1.0 / Test Plan v1.0

Project SH-2026-10, Team 10. This file records where the implementation differs from the specification, why, and which section is affected. Section (b) covers Anshul's modules (PRs #42–#49). Section (c) lists things noticed in other members' code; those are **for the owner to confirm** and haven't been changed.

Last updated: 30 Oct 2026.

## (a) Schedule

| # | What differs | Why | Spec reference |
|---|---|---|---|
| A1 | Sprint 1 issues (#3, #4, #9, #11, #12, #14) and Sprint 2 issues (#17, #29) were all implemented on 30 Oct 2026. They were planned for Sprint 1 (19–23 Oct) and Sprint 2 (26–30 Oct). Commit dates are the real dates; nothing is backdated. | Work was compressed into one day. My issues also had to wait for the integration PR #41 to be merged into main, which happened on 30 Oct. | SRS cover page / SAD cover page (Agile, 6 sprints); Test Plan §9 (testing tasks per sprint); sprint field of each GitHub issue |

## (b) Anshul's modules

### Lexer (FR-002, C-02)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| B1 | **Shared state is `Shell`, not `ShellContext`.** Built-ins take `(Shell *, int argc, char **argv)` instead of `builtin_run(BuiltinFn, Command *, ShellContext *)`. `expand()` takes `const Shell *`. | The existing code defines `Shell` and this built-in signature. The rule was to follow existing names where they differ from the SAD. | SAD §9.1 (`ShellContext`), §10.2 (C-03, C-08 APIs) |
| B2 | **`ShError.msg` is a fixed string, never a formatted buffer**, and only `ERR_NONE`, `ERR_SYNTAX` and `ERR_NOMEM` exist so far (in `include/error.h`). | No caller needs a formatted message yet, and a fixed string needs no freeing. More codes can be added when parse/redirect start using `ShError`. | SAD §10.1 (conventions), §10.2 (`lex()` errors) |
| B3 | **qmask holds one extra kind of information.** Backslash-escaped characters, and text inserted by `$VAR` expansion, use the same value as single-quoted characters (1 = always literal). The qmask also has one extra slot after the last character, which records whether the word contained any quotes, so `''` and `""` count. | The expander must not expand `\$HOME` or re-expand a substituted value. It also needs to keep `''$UNDEF` as one empty argument, as bash does. The SAD defines only 0/1/2 per character. | SAD §9.1 (`Token.qmask`) |
| B4 | **The lexer's output reaches the executor through an adapter, not a parser.** `lexer_tokenize()` is kept as a thin adapter over `lex()`: it turns tokens into an argv where operators are marked by a leading byte (`\x1f`, or `\x1e` for `&`). `|` is passed through as a plain word. A word that *starts* with one of those bytes is refused as `syntax error: invalid control character` (status 2). | The executor and redirection code (other owners) consume this marker argv, and `parse()` doesn't exist yet (see C2). Before the fix in #49, a quoted word or `$VAR` value starting with a marker byte acted as an operator: `'\x1f>' out` redirected output to `out`, a SEC-002 issue. | SAD §4.1 (lex → expand → parse → execute data flow), §5.2 (ownership), §10.2 (`parse()`); SRS SEC-002 |
| B5 | **Backslash-newline joins lines.** Outside quotes, a backslash before a newline removes both, as in POSIX. A backslash at the very end of input is kept literally. | The SRS doesn't cover these cases. Without the rule, the escaped newline at the end of a line would become part of the last word. | SRS FR-002; SAD §9.2 (lexer DFA) |

### Prompt and input loop (FR-001, SEC-004, C-01)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| B6 | **Input is read with the existing `pselect()`/`read()` loop in `main.c`, not `getline()`.** There's no separate `repl_run()`; only `prompt_render()`, `prompt_format()`, `line_is_blank()` and `privileges_elevated()` are in `repl.c`. | The existing loop already grows its buffer as needed (SEC-001), and it handles SIGCHLD without losing partly read input (NFR-003). Replacing it would have meant rewriting job-control code. | SAD §5.1 (C-01: `repl_run()`, `prompt_render()`), §9.3 (main control flow, `getline`), §7.2 |
| B7 | **64 KiB lines are verified through piped stdin, not by typing.** | In interactive mode, Linux's terminal driver limits a typed line to 4095 bytes. That's a kernel limit, not the shell's. The shell uses the same reader for both. | SRS FR-001; Test Plan ST-REPL-01 |
| B8 | **The job-control signals are ignored before taking the terminal.** In `initialize_terminal()`, SIGINT/SIGTSTP/SIGQUIT/SIGTTIN/SIGTTOU are now ignored *before* `setpgid()`/`tcsetpgrp()`, instead of after. | When a parent without job control starts the shell (`sh -c simple_shell`, or `script` in the CI images), `setpgid()` puts it in a background group. The next `tcsetpgrp()` then raised SIGTTOU and stopped the shell before its first prompt. This is the order the glibc manual gives. | SAD §9.3 (signal dispositions); SRS FR-001, FR-008 |
| B9 | **The setuid test (SEC-T04) only runs as root.** `tests/sec_t04.sh` makes setuid/setgid copies owned by `nobody` only when run as root. That happens in the CI Docker build, where it passes. Run as another user, it skips with a message, and a unit test covers the check. | Making a binary setuid to another user needs root. | Test Plan SEC-T04; SRS SEC-004; SAD SD-03 |

### Expansion (FR-003, C-03)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| B10 | **Aliases are expanded after variables, on the argv.** Alias expansion stays in `alias_expand()` (C-09), not in `expand()`. As a result, `$VAR` inside an alias's value isn't expanded. | The alias code already worked on argv and has its own tests. Moving it into the expander would change another member's module. | SAD §10.2 (`expand()`: alias first, then `$VAR`), §12 |
| B11 | **Only `$NAME` and `$?` expand.** Any other `$` is literal: `$` alone, `$1`, `$-`, `${NAME}`, and a `$` that crosses a quote boundary (`"$"HOME`). | FR-003 defines only `$NAME` and `$?`. | SRS FR-003 |
| B12 | **An empty redirect target is a syntax error.** If `$UNDEF` was the target of a redirect (`cmd > $UNDEF`), the word vanishes and the command fails as a syntax error (status 2). Bash reports "ambiguous redirect" (status 1). | UT-EXP-01 says an unquoted empty expansion produces no argument. A `>` left without a target is then E-SYN-02. | SRS FR-003; Test Plan UT-EXP-01; SAD §11.3 (E-SYN-02) |

### Environment built-ins (FR-009b, C-08/C-10)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| B13 | **Some cases the SAD doesn't define:** `export NAME` for an unset name only checks the name (there's no "exported but unset" state). `export` with no arguments lists the environment as `export NAME=value`. `env` with arguments returns status 2 ("too many arguments"). | The environment is kept with `setenv`/`unsetenv`, where every set variable is already exported. The SAD only defines `export NAME=value \| NAME`, and `env` without arguments. | SAD §10.3 (built-in interface) |
| B14 | **The built-ins live in `src/env.c`.** `builtin_export`/`unset`/`env` are there, alongside the C-10 `env_*` API, and are registered in the `builtins.c` table. | This keeps my built-ins in my own file; `builtins.c` gets only the 3 table entries. | SAD §5.1 (C-08 `builtins.c`, C-10 `env.c`) |

### Errors (NFR-007, C-10)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| B15 | **`sh_error()` is used only in Anshul's modules so far** (`env.c`, `main.c`, and the lexer/expander lines in `shell.c`). Other modules still print with `fprintf`/`perror`. Their messages that don't follow the format are listed in PR #45 for their owners. | The scope was limited to my own files; I didn't change other members' code. | SAD §10.1 ("no API prints except `sh_error()`"), §11.2; SRS NFR-007 |

### Security tests (SEC-001, SEC-002)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| B16 | **The whole-shell fuzz run is sandboxed.** In `make fuzz`, the corpus leaves out `/` and `.`, the shell runs with a PATH that points nowhere, and it works in a temp directory with `HOME` there too. The in-process lexer/expander fuzzer uses all bytes except NUL. | Random lines could otherwise run real programs or write files outside the test directory. | Test Plan SEC-T01; SRS SEC-001 |
| B17 | **Tests are shell scripts and assert-based C programs in `tests/`**, run by `make test`, `make fuzz` and `make check-injection`. They don't use Unity or the `tests/unit\|integration\|negative\|fuzz/` layout. | This follows the test style already in the repository. | SAD §13 (planned source layout); Test Plan §9 (Sprint 1: "Set up Unity") |

## (c) Noticed in teammates' code (for the owner to confirm)

These were found while working on my issues and haven't been changed. Each needs its owner to confirm whether it's a deviation to record or a defect to fix.

| # | What differs | Why it matters | Spec reference | Owner |
|---|---|---|---|---|
| C1 | **Pipelines (FR-005) are missing from main.** Merging PR #41 removed `src/executor.c`, `src/repl.c` and `tests/run_tests.sh` from PR #40, so `echo a \| tr a A` prints `a \| tr a A`. *For the owner to confirm.* | Two of my tests are pending until it's restored: SEC-005 `export Z=1 \| cat` (#44) and NEG `\| ls` (#45). Until then, `export Z=1 \| cat` changes the shell's environment. | SRS FR-005, SEC-005; SAD C-04/C-05, SD-01, §7.2 | Archana |
| C2 | **There's no parser.** There's no `parse()`/`Pipeline`/`Command`; the executor works on the lexer's marker argv (see B4). *For the owner to confirm.* | The SAD's lex → expand → parse → execute flow, and parser-level syntax errors such as E-SYN-02 "syntax error near `\|`", aren't there yet. Today's messages just say `syntax error`. | SAD §4.1, §9.1 (`Command`, `Pipeline`), §10.2 (C-04 `parse()`/`pipeline_free()`), §11.3 (E-SYN-02) | Archana |
| C3 | **History is saved in non-interactive mode.** `main.c` calls `history_add()` for every line and `history_save()` on exit, even for piped or scripted input. *For the owner to confirm.* | The SAD only adds and saves history when interactive. Scripts therefore write to `~/.simple_shell_history`. (Blank lines are no longer added, since #43.) | SAD §9.3 (`if (ctx->interactive) history_add/history_save`); SRS FR-010 | Atishay |
| C4 | **The negative suite has 23 cases, against the ≥ 30 NEG-01 requires.** `tests/negative.sh` (#45) has 23 cases plus `\| ls` pending. Some listed inputs aren't covered yet: interrupted system calls, and empty pipeline sides beyond `\| ls`. *For the owner to confirm.* | NEG-01 asks for ≥ 30 inputs in `tests/negative/*.in`. The NFR-004 robustness suite (#26) can build on `tests/negative.sh`. | SRS NFR-004; Test Plan NEG-01 | Archana (#26) |
