# Deviations from SRS v1.1 / SAD v1.0 / Test Plan v1.0

Project SH-2026-10, Team 10. This file records where the implementation differs from the specification, why, and which section is affected. Section (a) covers the whole team's schedule. Section (b) covers Anshul's modules (PRs #42–#49). Section (c) lists things noticed in other members' code, with their current status. Sections (d)–(f) cover Archana's, Atishay's and Ashlesh's modules; Anshul drafted them and each owner confirms them.

Last updated: 14 Nov 2026.

## (a) Schedule

Sprint 1 was planned for 19–23 Oct and Sprint 2 for 26–30 Oct. All dates below are the real commit and merge dates from `git log`; nothing is backdated.

| # | What differs | Why | Spec reference |
|---|---|---|---|
| A1 | **Anshul:** Sprint 1 issues (#3, #4, #9, #11, #12, #14) and Sprint 2 issues (#17, #29) were all committed on 30 Oct 2026 and merged on 1 Nov (PRs #42–#49). | Work was compressed into one day. These issues also had to wait for the integration PR #41 to be merged into main, which happened on 30 Oct. | SRS cover page / SAD cover page (Agile, 6 sprints); Test Plan §9 (testing tasks per sprint); sprint field of each GitHub issue |
| A2 | **Atishay:** Sprint 1 issues (#8, #10, #15, #16) and Sprint 2 issues (#21, #24, #27, #28) were all committed on 26 Oct. They reached main through the integration PR #41 on 30 Oct, not through separate PRs. | *Reason to be added by Atishay.* | As A1 |
| A3 | **Ashlesh:** Sprint 1 issues (#2, #7, #13) and Sprint 2 issues (#18, #19, #20, #25) were committed on 28–29 Oct. PRs #31–#33 were merged; the job-control PRs #34–#37 were closed and their commits reached main through the integration PR #41. | *Reason to be added by Ashlesh.* | As A1 |
| A4 | **Archana:** the setup (#1) was started on 3 Oct; #38–#41 were merged on 30 Oct. Sprint 2 work landed after the planned end of Sprint 2: pipelines v2 (#6, PR #51) on 1 Nov, and #23, #26 and #22 (PRs #52–#54) on 9–10 Nov. | *Reason to be added by Archana.* (PR #51 restored FR-005 after PR #41 removed it; see C1.) | As A1 |
| A5 | **Test reports and sprint videos** are produced after Sprint 2, from the frozen main. Their real dates are shown in the files. | The reports have to match the final code, so they come after the code freeze. | Test Plan §9; project deliverables (Sprint 1 and Sprint 2 videos) |

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

## (c) Noticed in teammates' code

These were found while working on Anshul's issues and were not changed by Anshul. Status as of 12 Nov, checked against main at `9430ab6`. Rows still open need their owner to confirm whether each is a deviation to record (in their own section below) or a defect to fix before the code freeze.

| # | What differs | Status | Spec reference | Owner |
|---|---|---|---|---|
| C1 | **Pipelines (FR-005) were missing from main** after PR #41 removed `src/executor.c`, `src/repl.c` and `tests/run_tests.sh` from PR #40. | **Resolved by PR #51** (1 Nov): `src/pipeline.c` restores 2–16 stage pipelines. The pending SEC-005 case (`export Z=1 \| cat`) and NEG case (`\| ls`) now run unconditionally. | SRS FR-005, SEC-005; SAD C-04/C-05, SD-01, §7.2 | Archana |
| C2 | **There's no `parse()`/`Command` as in the SAD.** | **Partly resolved by PR #51.** A `Pipeline` of `PipeStage {argv, argc}` now exists (`include/pipeline.h`), built by `pipeline_split()` from the lexer's marker argv (see B4) rather than by a `parse()` over tokens. An empty pipe side now gives `syntax error: near unexpected token '\|'` (E-SYN-02). Other syntax errors still print just `syntax error`. *Owner to confirm and record.* | SAD §4.1, §9.1 (`Command`, `Pipeline`), §10.2 (C-04 `parse()`/`pipeline_free()`), §11.3 (E-SYN-02) | Archana |
| C3 | **History is used in non-interactive mode.** | **Partly resolved by PR #54.** Script files (`simple_shell file.sh`) no longer load, add to or save history. Piped stdin (`echo ls \| simple_shell`) still does, because `main.c` checks `!script_mode`, not `sh.interactive`. The SAD only uses history when interactive. *Owner to confirm: fix before the freeze or record.* | SAD §9.3 (`if (ctx->interactive) history_add/history_save`); SRS FR-010 | Atishay / Archana (#54) |
| C4 | **The negative suite had 23 cases, against the ≥ 30 NEG-01 requires.** | **Resolved by PR #53** (9 Nov): `tests/negative.sh` has 42 cases. The inputs are inline in the script, not in `tests/negative/*.in` as NEG-01 describes. *Owner to record the layout difference.* | SRS NFR-004; Test Plan NEG-01 | Archana (#26) |

## (d) Archana's modules

FR-004, FR-005, FR-012, NFR-001, NFR-004, SEC-005, SETUP. Drafted by Anshul (issue #56) by comparing the code on main at `6d66961` with SRS v1.1, SAD v1.0 and Test Plan v1.0; each row was checked against the code, and most by running the shell. A *Why* that says "*Archana to confirm.*" is not evident from the code, comments or commit messages, so it is left to Archana; nothing there is guessed. Archana may correct any row.

### External commands (FR-004, C-05)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| D1 | **An unknown command prints the system error, not "command not found".** `nosuchcmd` gives `simple_shell: nosuchcmd: No such file or directory` (status 127 is correct). This is the same for a single command (`shell.c`) and for a pipeline stage (`pipeline.c`). `tests/pipeline.sh` checks for this text. The executor from PR #39 printed `command not found`. That code was removed in PR #41. | *Archana to confirm.* | SRS FR-004 (§5.3); SAD §11.3 (E-EXE-01); Test Plan IT-EXE-02 |
| D2 | **There's no `execute_pipeline()` or `resolve_path()`.** `execvp()` does the PATH search. A single command runs through `run_external()` in `shell.c`, and a pipeline through `pipeline_run_stages()` in `pipeline.c`. `shell_execute_line()` frees the `Pipeline` itself. It is never handed over to a job. (For the missing `parse()`/`Command`, see C2.) | *Archana to confirm.* | SAD §5.1 (C-05 interface), §5.2 (ownership), §8.1 (PATH lookup with `access(X_OK)`), §10.2 |

### Pipelines (FR-005, SEC-005, C-04/C-05)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| D3 | **A pipeline has one extra process.** The shell forks one child, which becomes the process-group leader. That child forks every stage and waits for them. The shell waits only for this child. All stages are still in one process group. | So Ctrl+C, Ctrl+Z, `fg`, `bg` and the job table treat a pipeline like any single process (comment in `shell.c`). The job table keeps one pid per job (`include/jobs.h`). | SRS §4.2.1 (UC-03 steps 3 and 5); SAD §3 ("no extra processes"), §8.1 ("single fork per stage; no extra shell process"), §9.1 (`Job.pids`, `npids`) |
| D4 | **Stages that already started are not killed if a later `pipe()` or `fork()` fails.** The error is printed and the status is 1. The started stages are closed off and reaped, but not sent SIGKILL. | *Archana to confirm.* | SRS §4.2.1 (UC-03 E2); SAD §11.3 (E-SYS-01), §11.4 |
| D5 | **The limit is exactly 16 stages.** With 17 or more, nothing runs, `simple_shell: too many pipeline stages (max 16)` is printed and the status is 2. | `Pipeline` holds a fixed array of `MAX_PIPELINE_STAGES` (16) stages (`include/pipeline.h`). The SRS asks for at least 16. | SRS FR-005; SAD §9.1 (`Pipeline`), §11.3 (error not listed) |
| D6 | **IT-PIP-01 is run with different checks.** `tests/pipeline.sh` tests 2, 3, 4 and 16 stages, `$?` of the last stage, and that no child is left over. It does not run `seq 1 100000 \| grep 7 \| sort -n \| tail -1`, time the 16-stage case (< 1 s), or list `/proc/<pid>/fd` afterwards. | *Archana to confirm.* | Test Plan IT-PIP-01 |
| D7 | **SEC-T05 is split, and one of its steps isn't run.** The pipeline steps are in `tests/pipeline.sh`. The `&` steps for `export`/`unset` are in `tests/env_builtins.sh` (from PR #44). No test runs step 3's `cd / &`. Run by hand, it leaves the directory unchanged. | *Archana to confirm.* | Test Plan SEC-T05; SRS SEC-005 |

### Script mode (FR-012, C-01)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| D8 | **Job control is still on when the shell isn't interactive.** In script mode and with piped stdin, each command and pipeline still gets its own process group. `cmd &` still prints `[1] pid`, and `jobs`/`fg`/`bg` still work. Only the terminal handover and the ignored signals are skipped. `tests/pipeline.sh` checks the `[1] pid` line with piped stdin. | *Archana to confirm.* | SRS FR-012 ("disable job control"), §3.4; SAD §9.3 (last paragraph) |
| D9 | **Lines whose first non-blank character is `#` are skipped as comments, in every mode.** Like blank lines (BR-5), they don't change `$?` or history. So an interactive `# note` is not saved in history. A `#` later in a line is not a comment: `echo a # b` prints `a # b`. | A script's `#!` first line has to be skipped (comment in `main.c`). For interactive mode: *Archana to confirm.* | SRS FR-012, FR-010 ("each non-empty input line"), BR-5; SAD §9.3 |
| D10 | **Some cases the spec doesn't define:** a missing script gives status 127, and a directory or unreadable script gives 126 (with the system error message). More than one argument gives `simple_shell: usage: simple_shell [script]`, status 2. | These reuse the §3.1 status codes (`ST_NOTFOUND`, `ST_NOEXEC`, `ST_SYNTAX` in `include/error.h`). The SAD only defines `simple_shell` and `simple_shell script.sh`. | SAD §10.4; SRS §3.1 |

(History with piped stdin: see C3.)

### Dispatch overhead (NFR-001)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| D11 | **PT-01 is measured in-process, not with a script.** `tests/test_dispatch.c` calls `shell_execute_line("true")` 200 times per round, against a bare fork + `execvp` + `waitpid` loop. It takes the best of 5 rounds, not the median. It runs as part of `make test` on whatever machine runs it, not in the reference container (2 vCPU, 4 GB). Reading lines, start-up and the binary itself are not timed. | The best round is used so a busy machine doesn't fail the test (comment in the test). For the rest: *Archana to confirm.* | SRS NFR-001; Test Plan PT-01, §10 |

### Negative suite (NFR-004)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| D12 | **There's no input for interrupted system calls**, one of the kinds NFR-004 lists. Also, "the shell survives" is checked by running a next line (`echo alive`) on piped stdin, not by seeing the prompt again. (For the inline inputs instead of `tests/negative/*.in`, see C4.) | With piped stdin there is no prompt (FR-012), so a next line that runs stands in for it. For the interrupted calls: *Archana to confirm.* | SRS NFR-004; Test Plan NEG-01 |

### Setup (SETUP, #1)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| D13 | **The source layout doesn't match the plan.** There is no `parser.c` or `executor.c`. C-04 and C-05 are in `src/shell.c` (`shell_execute_line()`, `run_external()`) and `src/pipeline.c`. The setup PR #38 created `src/executor.c`, `src/repl.c` and `tests/run_tests.sh`, and PRs #39–#40 built on them. The integration PR #41 removed all three (see C1). The `ShellContext` from #38 became `Shell` (see B1). For the test layout, see B17. | *Archana to confirm.* | SAD §13 (planned source layout), §5.1 (C-04 `parser.c`, C-05 `executor.c`) |

## (e) Atishay's modules

FR-009a, FR-010, FR-011, SEC-006, SEC-007, NFR-002, NFR-005, NFR-008. Drafted by Anshul (issue #56) by comparing the code on main at `6d66961` with SRS v1.1, SAD v1.0 and Test Plan v1.0; each row was checked against the code, and most by running the shell. A *Why* that says "*Atishay to confirm.*" is not evident from the code, comments or commit messages, so it is left to Atishay; nothing there is guessed. Atishay may correct any row.

### Built-ins cd, pwd, echo, exit (FR-009a, C-08)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| E1 | **`cd -` is not supported.** The shell doesn't remember the previous directory (there's no `prev_cwd` in `Shell`, and `OLDPWD` isn't set). `cd -` tries a directory named `-` and fails with status 1. | *Atishay to confirm.* | SRS FR-009; SAD §9.1 (`ShellContext.prev_cwd`), §10.3 (`cd [dir \| ~ \| ~/path \| -]`); Test Plan UT-BI-01 |
| E2 | **`cd ~` and `cd ~/path` are not supported.** Neither `cd` nor the expander replaces `~`, so it is taken as a directory name and `cd` fails with status 1. Only `cd` with no argument goes to `$HOME`. | *Atishay to confirm.* | SRS FR-009; SAD §10.3; Test Plan UT-BI-01, ST-E2E-01 (`cd ~/e2e`) |
| E3 | **Some cases the SAD doesn't define:** `cd a b` and `exit 1 2` print "too many arguments" and return 1; after `exit 1 2` the shell keeps running. `cd` with `HOME` unset prints "HOME not set" (status 1). `exit abc` prints "numeric argument required" and the shell exits with 2. `exit n` exits with `n & 0xff` (`exit 256` → 0, `exit -1` → 255). `pwd` ignores its arguments. `echo` only treats one leading `-n` as an option (`echo -n -n x` prints `-n x`) and has no other options. | *Atishay to confirm.* | SAD §10.3 (built-in interface) |
| E4 | **There's no `builtin_lookup()`.** `builtin_is(name)` checks a static name table, and `builtin_run(sh, argc, argv)` finds and calls the function by name. The signature itself is covered by B1. | *Atishay to confirm.* | SAD §10.2 (C-08 `builtin_lookup()`, `builtin_run()`) |

### History (FR-010, SEC-006, SEC-007, C-09)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| E5 | **There's no `history` built-in.** `history` is not in the built-in table, so it is run as an external command and fails with status 127. The history can't be listed from the shell; it is only kept in `~/.simple_shell_history`. | *Atishay to confirm.* | SRS §3.1, FR-009, FR-010, UC-07; SAD §10.3 (`history`); Test Plan IT-HIS-02, ST-E2E-01 |
| E6 | **Wider permissions are not fixed at load, and no warning is printed.** `history_load()` doesn't check the file's mode. The file is set to 0600 only when `history_save()` runs at exit, so a 0644 file stays 0644 for the whole session. New files are created 0600 as required. | *Atishay to confirm.* | SRS SEC-006; SAD §7.2 ("corrected with fchmod at load"); Test Plan SEC-T06 ("Warning printed") |
| E7 | **History load and save errors are silent.** An unreadable history file gives an empty history with no message. If saving fails (for example a file with mode 000), the session's history is lost with no message. `main.c` ignores both return values. | *Atishay to confirm.* | SAD §10.2 (C-09: load/save failures are warnings), §11 (E-HIS-01 "warning: cannot load history") |
| E8 | **With `HOME` unset there is no history.** `history_default_path()` returns NULL, and the shell neither loads nor saves a history file. | *Atishay to confirm.* | SRS §3.2 (`~/.simple_shell_history`), FR-010 |

History is also used when stdin is piped; see C3.

### Aliases (FR-011, C-09)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| E9 | **Alias names are not checked.** Any non-empty text before the first `=` is accepted as a name, such as `alias 'a b=echo'` or `alias /x=echo`. Only an empty name (`alias =x`) is refused. | *Atishay to confirm.* | SAD §10.2 (`alias_set`: names validated `[A-Za-z0-9_]+`), §10.3 (1: invalid name) |
| E10 | **Alias chains stop after 16 expansions.** A chain of more than 16 different aliases stops expanding, and the 17th name is run as a command. A self-referring alias is expanded once, as FR-011 requires, and a loop such as `a`→`b`→`a` stops when a name repeats. | Stopping at a repeated name keeps self-referring aliases such as `ls='ls -l'` from looping (comment in `alias.h`). The limit of 16: *Atishay to confirm.* | SRS FR-011 |

Alias expansion after `$VAR` is covered by B10.

### Data structures and APIs (C-09)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| E11 | **`History` and `AliasTable` differ from the SAD.** `History` is a fixed array `char *entries[1000]` with `start` and `count`, not `char **lines` with `cap`. `AliasTable` holds two arrays, `names` and `values`, with no `cap`, instead of an array of `Alias {name, value}`. `alias_remove()` returns -1, not 1, when the name is not found. `history_load()`/`history_save()` return -1, not `ERR_IO`. There are extra functions: `history_count()`, `history_get()` and `history_default_path()`. | *Atishay to confirm.* | SAD §9.1 (`History`, `Alias`, `AliasTable`), §10.2 (C-09 APIs) |

### Quality and performance checks (NFR-002, NFR-005, NFR-008, SEC-007)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| E12 | **The memory check doesn't run a regression script.** `make valgrind` (and `make asan`) feed a generated file of 1,000 `echo N > /dev/null` lines plus `alias`, an alias call and `unalias` to the shell on stdin. All of these are built-ins, so external commands, pipelines and background jobs are not checked under Valgrind. | *Atishay to confirm.* | SRS NFR-002; Test Plan MEM-01 (`valgrind … ./simple_shell regression_1000.sh`) |
| E13 | **`make lint` uses a different cppcheck run.** It enables `warning,style,performance,portability` (not `all`), checks `src` and `tests`, and fails on any finding, not only on errors. | *Atishay to confirm.* | SRS NFR-005; Test Plan QA-01 (`cppcheck --enable=all src/`) |
| E14 | **`make check-globals` is stricter than QA-01, and it currently fails.** It fails on any writable data symbol in any object, static or not, instead of checking `nm -g` for non-static globals outside `signals.c`. There is no `signals.c`. It now fails on the static SIGCHLD flag `child_event_pending` in `main.c`, which was added later (29 Oct). `make test`, the Dockerfile and CI don't run it. | Its comment says the aim is no mutable file-scope data in any module. Otherwise *Atishay to confirm.* | SRS NFR-005; Test Plan QA-01 (`nm -g`, "0 non-static globals outside signals.o") |
| E15 | **PT-02 is measured differently.** `tests/perf_pt02.sh` takes the best of 5 runs, not the median of 20. Stdin is `/dev/null`, so no prompt is shown. It times launch, history load, history save and exit, using `python3` timestamps. It only loads the history because history is used when stdin isn't a terminal (see C3). | The script's comment says start-up is measured as launch + load history + exit on EOF. The rest: *Atishay to confirm.* | SRS NFR-008; Test Plan PT-02 (time to first prompt, median of 20 runs) |
| E16 | **SEC-T07 is checked without an interactive session.** `tests/sec_t07.sh` pipes in a probe script that counts `/dev/fd` entries, and compares the count with and without a history file. It doesn't run `ls -l /proc/self/fd` interactively. Like E15, it depends on history being loaded when stdin is piped (see C3). | *Atishay to confirm.* | SRS SEC-007; Test Plan SEC-T07 |
| E17 | **Some planned tests are missing.** There are no automated tests for IT-HIS-02 (needs the `history` built-in, E5) or SEC-T06 (needs the warning at load, E6). `tests/test_history.c` only checks that saving resets a 0644 file to 0600. UT-BI-01 doesn't cover `cd -` or `cd ~` (E1, E2). | *Atishay to confirm.* | Test Plan IT-HIS-02, SEC-T06, UT-BI-01 |

## (f) Ashlesh's modules

FR-006, FR-007, FR-008, FR-009c, SEC-003, NFR-003, NFR-006. Drafted by Anshul (issue #56) by comparing the code on main at `6d66961` with SRS v1.1, SAD v1.0 and Test Plan v1.0; each row was checked against the code, and most by running the shell. A *Why* that says "*Ashlesh to confirm.*" is not evident from the code, comments or commit messages, so it is left to Ashlesh; nothing there is guessed. Ashlesh may correct any row.

### Redirection (FR-006, SEC-003, C-06)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| F1 | **Redirection is in `src/redirection.c` with its own API.** `redirection_extract()` takes the redirections out of the lexer's marker argv (see B4) into a `RedirectionList` of `{kind, path}` actions. `redirection_apply()` saves fds 0–2 and applies the list, `redirection_restore()` puts them back, and `redirection_free()` frees the list. There's no `apply_redirections(Command *, ShError *)`, `save_std_fds()` or `restore_std_fds()`, and no `input_file`/`output_file`/`append_output`/`error_file` fields. Errors are printed directly, not returned in a `ShError`. | There is no `parse()`/`Command` to attach redirections to (see C2). *Ashlesh to confirm.* | SAD §5.1 (C-06 `redirect.c`), §9.1 (`Command`), §10.2 (C-06 APIs), §13 (source layout) |
| F2 | **For a command without a pipe, redirections are applied in the shell process before `fork()`.** This includes external commands and background jobs, not only built-ins. The child inherits the redirected fds, and the shell restores its own fds afterwards. The saved copies are marked `FD_CLOEXEC`, so children don't inherit them. | *Ashlesh to confirm.* | SRS §4.3 step 7 (fork, then apply redirections in the child); SAD Fig. 9.2 (SD-01), §10.2 (`apply_redirections()` pre-condition) |
| F3 | **A command can have several redirections, in any position.** They're opened left to right, and the last one for each stream wins (`echo a > x > y` writes to `y`). If a later one fails, files opened before it have already been created or truncated. A redirection can also come before the command word (`> f echo hi`). | *Ashlesh to confirm.* | SAD §9.1 (one field per stream in `Command`), §10.4 (grammar: `command := WORD+ ( redirect )*`), §11.1 ("fail before side effects") |
| F4 | **A missing or invalid redirect target prints only `simple_shell: syntax error`** (status 2), with no "near ..." part. Examples: `echo hi >`, `echo a > > b`, and a command made only of redirections. | *Ashlesh to confirm.* See also C2. | SAD §11.3 (E-SYN-02) |

### Background jobs and the job table (FR-007, NFR-003, C-07)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| F5 | **Job IDs are never reused.** The table keeps a counter that only goes up. After job 1 ends, the next job is still `[2]`, even when the table is empty. | *Ashlesh to confirm.* | SRS FR-007 ("reusing the lowest free ID"); SAD §10.2 (`job_add()`: lowest free `job_id`) |
| F6 | **Finished jobs are never reported.** There's no `[n]+ Done <command>` message and no `JOB_DONE` state: a finished job is removed from the table without a message. A background job that stops (for example on SIGTTIN) is marked Stopped without a message too. | *Ashlesh to confirm.* | SRS FR-007, Fig. 4.3; SAD §9.5, §10.2 (`jobs_reap()` prints Done/Stopped); Test Plan IT-JOB-01 |
| F7 | **The job table is an opaque `JobTable`, not a `Job *jobs` list in the shell context.** Each job keeps only `id`, `pid` (the job's process-group leader), `command`, `state` (RUNNING or STOPPED) and `next`. There's no `pgid`, `pids`/`npids`, `is_foreground` or `last_status`. Terminal modes are not saved (`shell_tmodes`). The functions are `job_table_add/add_stopped/find/latest_id/set_state/remove/reap/print_status`, not `job_add()`, `wait_foreground()` and `jobs_reap()`. | *Ashlesh to confirm.* | SAD §9.1 (`Job`, `ShellContext`), §10.2 (C-07 APIs) |
| F8 | **Reaping calls `waitpid()` on each job's pid**, with `WNOHANG \| WUNTRACED \| WCONTINUED`, instead of `waitpid(-1, &status, WNOHANG \| WUNTRACED)`. It runs from the input loop whenever the SIGCHLD flag is set, and before each prompt. | *Ashlesh to confirm.* | SRS BR-3; SAD §10.2 (`jobs_reap()`) |

### Signals and foreground jobs (FR-008, C-07)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| F9 | **There's no `signals.c` and no `signals_init()`.** The SIGCHLD flag and handler are `static` in `main.c`. Taking the terminal and ignoring the job-control signals is done in `initialize_terminal()` in `main.c` (see B8). Children reset the signals in `shell.c`. The SIGCHLD handler is installed without `SA_RESTART`, and the loops retry on `EINTR`. | *Ashlesh to confirm.* | SAD §5.1 (C-07 `jobs.c`, `signals.c`), §9.3 (SIGCHLD with `SA_RESTART`), §10.2 (`signals_init()`), §13; SRS NFR-005 |
| F10 | **The stop message has no `+`.** Ctrl+Z prints `[1] Stopped sleep 30`, not `[1]+ Stopped sleep 30`. `$?` is then 128 + the stop signal (148 for SIGTSTP). | *Ashlesh to confirm.* | SRS FR-008; Test Plan ST-SIG-02 |
| F11 | **Job control is not turned off in non-interactive mode.** Every child still gets its own process group, stopped children go into the job table, `fg`/`bg` still work, and the SIGCHLD handler is installed. In script mode on a terminal this shows: a command that reads the terminal (`cat`) is stopped by SIGTTIN (`[1] Stopped cat`, `$?` = 149). Ctrl+C during a script kills the shell, but the running command keeps going. | *Ashlesh to confirm.* | SRS §3.4 ("job control disabled" when input is not a TTY), FR-012; SAD §9.3 ("in script mode job control is disabled and signals keep default dispositions") |

### Job built-ins (FR-009c, C-08)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| F12 | **`jobs`, `fg` and `bg` refuse to run with `&`.** They print `simple_shell: fg cannot run in the background` and return 1, instead of running in a child process. | *Ashlesh to confirm.* | SRS FR-009 (with `&` built-ins run in a child), BR-1 |
| F13 | **`jobs` prints `[1] Running sleep 2 &`**: no `+`, and the command text includes the `&`. | *Ashlesh to confirm.* | Test Plan IT-JOB-01 (`[1]+ Running sleep 2`) |
| F14 | **An unknown job gives `simple_shell: fg: no such job: 3`** instead of `fg: %3: no such job`. The status is 1, as specified. | *Ashlesh to confirm.* | SAD §10.3, §11.3 (E-JOB-01) |
| F15 | **Some cases the SAD doesn't define:** `fg`/`bg` with no argument use the most recently added job, or print `no current job` (status 1). A job can be given as `%1` or `1`. An invalid job ID prints `invalid job ID` (status 1). Extra arguments give a usage error (status 2), including `jobs` with any argument. `bg` prints `[n] <command> &`. | *Ashlesh to confirm.* | SAD §10.3 (built-in interface) |

### Tests and CI (NFR-003, NFR-006, SEC-003)

| # | What differs | Why | Spec reference |
|---|---|---|---|
| F16 | **The zombie test starts one background job, not 50.** `tests/zombie_processes.sh` checks that child's state once, through `/proc`, after 1 s. ZMB-01 asks for 50 jobs sampled every 50 ms for 2 s. Run by hand with 50 `sleep 0.1 &` jobs, no zombie was seen. | `/proc` is used instead of `ps` so the test works on the Debian image (commit f5d6cc0; comment in the test). The one-job scale: *Ashlesh to confirm.* | Test Plan ZMB-01; SRS NFR-003 |
| F17 | **SEC-T03 uses the tester's own files.** It writes to a mode 444 file and reads a mode 000 file. In CI, which runs as root, the shell is run as `nobody` with `runuser`. There are no `tester`/`other` users and no `/home/other/secret`. The test checks for a non-zero status and some stderr output, not for `$?` = 1 and the exact message. By hand, both are as specified (`simple_shell: ro.txt: Permission denied`, status 1, file unchanged). | Running as `nobody` is because "root-based CI must run the checks as an unprivileged user" (comment in `tests/sec_t03.sh`). The rest: *Ashlesh to confirm.* | Test Plan SEC-T03, IT-RED-02, §10 (users `tester` and `other`); SRS SEC-003 |
| F18 | **IT-JOB-01, ST-SIG-01 and ST-SIG-02 have no automated test.** There's no pty/`expect` test for Ctrl+C, Ctrl+Z, `bg` and `fg`. `tests/test_job_control.c` drives `jobs`/`fg`/`bg` on a child stopped with SIGSTOP. `tests/background_jobs.sh` checks the `[1] pid` and `[2] pid` lines and that the shell isn't blocked (under 2.5 s, against the < 100 ms in IT-JOB-01). | *Ashlesh to confirm.* (Test Plan §12 allows running ST-SIG locally if the CI pty is unreliable.) | Test Plan IT-JOB-01, ST-SIG-01..02, §12 |
| F19 | **The CI images have fewer tools than planned.** The Dockerfile installs only `build-essential`, `cppcheck` and `valgrind`, with no `clang-tidy`, `expect` or `lcov`, and CI has no Clang-Tidy step. All checks run inside `docker build`, as root. The Ubuntu 22.04 / Debian 12 matrix itself is as specified. | *Ashlesh to confirm.* | SAD §6; SRS §2.5 (CI: Cppcheck/Clang-Tidy); Test Plan §10 (Docker images), PORT-01 |
| F20 | **CI doesn't run `make check-globals`.** It was removed from the Dockerfile in commit c1d9ff4 ("Fix Docker CI checks"). The check currently fails on `child_event_pending` (`main.o`) and `table` (`builtins.o`). | *Ashlesh to confirm.* | Test Plan QA-01; SRS NFR-005 |
