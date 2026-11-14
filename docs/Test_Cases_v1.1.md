# Test cases v1.1 (after Sprints 1 and 2)

> **Draft, 14 Nov 2026.** To be rechecked against the frozen main after the Sprint 2 code freeze.

Project SH-2026-10, Team 10. This updates Test Plan v1.0 Section 14 to what the tests actually do on main at `6d66961`, with DEVIATIONS rows from PR #58.
Each case lists what it checks now and what changed from v1.0, with the `docs/DEVIATIONS.md` row.

Changes that apply to every case:
- Tests are plain shell scripts and assert-based C programs in `tests/`, not Unity, and not the `tests/unit|integration|negative|fuzz/` layout (B17).
- Integration and system tests feed the shell through piped stdin or a file. Only `tests/repl.sh` uses a pty (through `script`).
- The Actual Result and Status columns are filled from a real run later.

## 1. Test Plan cases

### UT-LEX-01: quotes and escapes (FR-002)
- **Now:** `tests/test_lexer.c` `test_quotes_and_escapes()`. `echo "a b" 'c $d' e\ f` gives 4 WORD tokens `[echo] [a b] [c $d] [e f]`. It also checks the qmask of each character.
- **Changed:** Nothing in the expected result. The qmask checks were added (B3).

### UT-LEX-02: unterminated quote (FR-002, NFR-004)
- **Now:** `tests/test_lexer.c` `test_unterminated()`. `lex("echo \"abc")` returns NULL with `ERR_SYNTAX` and the message "unterminated quote". Three more unterminated forms are checked. `tests/lexer.sh` runs it in the shell: status 2, exact stderr text, nothing on stdout, the command does not run.
- **Changed:** The shell-level check was added.

### UT-LEX-03: operators without spaces (FR-002)
- **Now:** `tests/test_lexer.c` `test_operators_no_spaces()`. `ls>o 2>e|wc&` gives WORD GREAT WORD ERR_GREAT WORD PIPE WORD AMP. `test_all_operators()` also covers `<`, `>>`, `&>` and quoted operators.
- **Changed:** No change.

### UT-EXP-01: variable expansion (FR-003)
- **Now:** `tests/test_expander.c`. With `HOME=/home/t` and `UNDEF` unset, `echo $HOME $UNDEF '$HOME'` gives `[echo, /home/t, $HOME]`.
- **Changed:** No change. Many extra cases were added (see Section 2).

### UT-EXP-02: `$?` expansion (FR-003)
- **Now:** `tests/test_expander.c`. With `last_status = 1`, `echo $?` gives `[echo, 1]`. `tests/expander.sh` checks `$?` after `false`, a missing command and `true` in the running shell.
- **Changed:** The shell-level check was added.

### UT-PAR-01: parse pipeline with redirections (FR-005, FR-006)
- **Now:** There is no `parse()`. `tests/test_lexer.c` `test_argv_adapter()` checks that `cat <in >>out 'x y' &` becomes the marker argv. `tests/test_pipeline.c` checks that `pipeline_split()` cuts an argv into stages, keeps the 16-stage limit and finds empty stages.
- **Changed:** The case is split across two files. There are no `input_file`, `output_file`, `append` or `is_background` fields to check (C2, F1, B4).

### IT-EXE-01: run an external command (FR-004)
- **Now:** External commands run in `tests/expander.sh` (`false`, `true`, `printf`), `tests/sec_t02.sh` (`printf`, exact output) and `tests/pipeline.sh` (`tr`, `sort`, `head`). `$?` is checked after each kind.
- **Changed:** `ls /` is not run and output is not compared with bash. Other external commands are used.

### IT-EXE-02: unknown and non-executable commands (FR-004, NFR-007)
- **Now:** `tests/negative.sh`: `nosuchcmd_xyz` gives status 127, `./noexec` (mode 644) gives 126, both with a `simple_shell: ` message, and the shell keeps running.
- **Changed:** The message is `simple_shell: nosuchcmd: No such file or directory`, not `command not found` (D1). The test checks the prefix, not the v1.0 text.

### IT-PIP-01: multi-stage and 16-stage pipelines (FR-005)
- **Now:** `tests/pipeline.sh`: 2-, 4- and 16-stage pipelines give the right output. `$?` is the last stage's status. No pipeline child is left after the pipeline ends. A 17th stage is refused.
- **Changed:** `seq 1 100000 | grep 7 | sort -n | tail -1` is not run. The 16-stage case is not timed (< 1 s), and `/proc/<pid>/fd` is not listed (D6, D5).

### IT-RED-01: all redirection operators (FR-006)
- **Now:** `tests/redirection.sh`: `>`, `>>`, `<`, `2>` and `&>` write or read the right files, and only the unredirected `echo` reaches stdout. A failed output open fails the command.
- **Changed:** `&>` is tested with `sh -c 'printf OUT; printf ERR >&2'` instead of `ls / /nope`.

### IT-RED-02: redirect to an unwritable file (FR-006, SEC-003)
- **Now:** `tests/sec_t03.sh` first part: `echo ... > protected.txt` (mode 444) fails, the file keeps its content, the command does not run, and stderr is not empty.
- **Changed:** It checks a non-zero status, not `$?` = 1, and not the exact `Permission denied` message. Under root it runs as `nobody` (F17).

### IT-JOB-01: background job (FR-007)
- **Now:** `tests/background_jobs.sh`: two `/bin/sleep 3 &` print `[1] pid` and `[2] pid`, and the shell goes on at once (< 2.5 s). `tests/pipeline.sh`: `jobs` shows `[1] Running  sleep 2 | cat &`.
- **Changed:** No `Done` message exists (F6). `jobs` has no `+` and shows the `&` (F13). The < 100 ms prompt time is not measured (F18).

### ST-SIG-01: Ctrl+C reaches only the child (FR-008)
- **Now:** No automated test.
- **Changed:** Moved to manual. There is no pty/expect test for Ctrl+C (F18).

### ST-SIG-02: stop and resume a job (FR-008, FR-007, FR-009)
- **Now:** `tests/test_job_control.c` stops a child with SIGSTOP. `jobs` lists `[1] Stopped`, `bg %1` sets it Running, and `fg` resumes it and returns its status (42).
- **Changed:** Ctrl+Z and Ctrl+C are not sent; that part is manual (F18). The stop line has no `+` (F10).

### UT-BI-01: `cd` variants (FR-009)
- **Now:** `tests/test_builtins.c`: `cd /tmp` returns 0 and changes the directory. `cd /no/such/dir` returns 1. `cd a b` returns 1.
- **Changed:** `cd -` and `cd ~` are not supported, so not tested (E1, E2, E17). `cd` with no argument and "cwd unchanged" are not checked. `/tmp` is used instead of `/usr`.

### IT-BI-02: built-ins run in the parent (FR-009)
- **Now:** `tests/expander.sh`: `export X=5`, `unset X`, `echo [$X]` prints `[]`, `cd /tmp; pwd` prints `/tmp`, `exit 3` ends the shell with 3. `tests/env_builtins.sh`: after `export X=5`, `env` has `X=5`; after `unset X`, it has no `X`.
- **Changed:** Split across two files. `env | grep ^X=` is replaced by `env > file` plus `grep` on the file.

### UT-HIS-01: ring buffer boundary (FR-010)
- **Now:** `tests/test_history.c`: 1,500 adds leave 1,000 entries, first `cmd500`, last `cmd1499`. Blank lines are not added.
- **Changed:** 1,500 lines instead of 1,001. The boundary is the same.

### IT-HIS-02: history persists across sessions (FR-010, SEC-006)
- **Now:** `tests/test_history.c`: `history_save()` creates the file with mode 0600, and `history_load()` gets the entries back. `tests/repl.sh`: a piped session writes its lines to `~/.simple_shell_history`.
- **Changed:** There is no `history` built-in, so session 2 can't list the history (E5, E17). The shell-level check works only because piped stdin uses history (C3).

### UT-ALI-01: alias set, expand, guard, remove (FR-011)
- **Now:** `tests/test_alias.c`: `ll` expands with its arguments. Self-referring `ls` expands once. The loop `a`→`b`→`a` stops. After `alias_remove("ll")`, `alias_get("ll")` is NULL.
- **Changed:** Values differ slightly (`ls -la`, `ls -G`). Removal is tested through `alias_remove()`, not the `unalias` built-in.

### ST-REPL-01: prompt format and long line (FR-001)
- **Now:** `tests/repl.sh`: a 64 KiB `echo` line is echoed in full, with no prompt and no stderr, through piped stdin. In a pty (`script`), the prompt shows `user@host:~/w$ `, then `~$ ` and `/$ ` after `cd`. `tests/test_prompt.c` checks `tester@box:~/w$ ` and other forms.
- **Changed:** The 64 KiB line goes through piped stdin, not typing (B7). The pty part skips if `script` is missing.

### ST-BAT-01: script mode (FR-012)
- **Now:** `tests/script.sh`: a script's output has no prompt. `exit 7` ends it with 7, and the last command's status is the exit status. Piped `echo piped` prints `piped`. A script writes no history file.
- **Changed:** Different scripts (`exit 7`, not the v1.0 `s.sh`). Piped stdin still writes history (C3). Missing, directory and unreadable scripts and extra arguments were added (D10).

### ST-E2E-01: realistic session (FR-001..FR-011)
- **Now:** No automated test.
- **Changed:** Not covered. As written, it can't pass: `cd ~/e2e` fails (E2), `history` doesn't exist (E5), and `jobs` output differs (F13). Its parts are tested in other files.

### PT-01: dispatch overhead (NFR-001)
- **Now:** `tests/test_dispatch.c`: `shell_execute_line("true")` 200 times against a bare fork + `execvp` + `waitpid`. The best of 5 rounds must be at most 2 ms per command.
- **Changed:** In-process, not a 1,000-line script. Best of 5, not median. Runs on any machine, not the reference container (D11).

### PT-02: start-up with full history (NFR-008)
- **Now:** `tests/perf_pt02.sh`: with a 1,000-entry history, launch + load + exit on EOF takes at most 100 ms (best of 5).
- **Changed:** Best of 5, not median of 20. Stdin is `/dev/null`, so there is no prompt; save and exit are also timed (E15, C3).

### MEM-01: leak check (NFR-002)
- **Now:** `make valgrind` and `make asan` run 1,000 `echo N > /dev/null` lines plus `alias`, an alias call and `unalias`. Both run in the Dockerfile, so in CI.
- **Changed:** A generated input of built-ins, not `regression_1000.sh`. External commands, pipelines and jobs are not run under Valgrind (E12).

### ZMB-01: zombie reaping (NFR-003)
- **Now:** `tests/zombie_processes.sh`: one `/bin/sleep 0.5 &`; after 1 s the child is not a zombie (state read from `/proc`). `tests/pipeline.sh`: no pipeline child is left.
- **Changed:** One job, not 50, and one sample, not every 50 ms for 2 s. `/proc` instead of `ps` (F16).

### NEG-01: negative suite (NFR-004, NFR-007)
- **Now:** `tests/negative.sh`: 42 cases. Each checks the status, empty stdout, a `simple_shell: ` prefix (or exact text), and that the next line (`echo alive`) still runs.
- **Changed:** Inputs are inline in the script, not `tests/negative/*.in` (C4). "Prompt re-shown" is replaced by "next line runs". No interrupted-system-call input (D12).

### QA-01: modularity and static analysis (NFR-005)
- **Now:** `make lint` runs cppcheck on `src` and `tests` and fails on any finding (in the Dockerfile, so in CI). `make check-globals` fails on any writable data symbol.
- **Changed:** cppcheck uses `warning,style,performance,portability`, not `all` (E13). `check-globals` is stricter, is not run by `make test` or CI, and currently fails (E14, F20).

### PORT-01: build matrix (NFR-006)
- **Now:** `.github/workflows/ci.yml` builds the Dockerfile on Ubuntu 22.04 and Debian 12. Each build runs `make` (`-Werror`), `make lint`, `make test`, `make valgrind` and `make asan`.
- **Changed:** The images have no clang-tidy, expect or lcov, and all checks run as root inside `docker build` (F19).

### SEC-T01: fuzzing with ASan/UBSan (SEC-001)
- **Now:** `make fuzz` (CI step): `tests/fuzz.c lex 1000` runs 1,000 random lines (one of 1 MB) through lex, expand and argv in-process. `tests/fuzz_shell.sh` feeds 1,000 lines to the sanitizer build of the whole shell. Any sanitizer report, crash or hang fails.
- **Changed:** The whole-shell run is sandboxed: no `/` or `.` in the corpus, an empty PATH, a temp directory (B16).

### SEC-T02: command injection (SEC-002)
- **Now:** `tests/sec_t02.sh`: `echo "x; rm /tmp/canary"` prints literally and the canary survives. Quoted `;`, `|`, `$( )`, backticks, `&&`, `>` and `&` reach `printf` literally. `make check-injection` passes and catches planted `system`, `popen` and `/bin/sh`. `tests/expander.sh`: `export V='$(rm /tmp/canary)'; echo $V` prints the text and the canary survives.
- **Changed:** Split across `sec_t02.sh` and `expander.sh`. The source search is a make target, also run in CI.

### SEC-T03: permission bypass (SEC-003)
- **Now:** `tests/sec_t03.sh`: writing to a mode 444 file and reading a mode 000 file both fail, the file is unchanged, and the command does not run. Under root it runs as `nobody`.
- **Changed:** Uses the tester's own files, not `/home/other/secret`. Checks a non-zero status and some stderr, not `$?` = 1 and the exact message (F17).

### SEC-T04: refuse setuid (SEC-004)
- **Now:** `tests/sec_t04.sh`: setuid (4755) and setgid (2755) copies owned by `nobody` exit 1 with `simple_shell: refusing to run with elevated privileges` and run nothing. `tests/test_prompt.c` checks `privileges_elevated()`.
- **Changed:** The real case runs only as root (the Docker build). As another user it skips and the unit test covers the check (B9). setgid was added.

### SEC-T05: environment isolation (SEC-005)
- **Now:** `tests/pipeline.sh`: `export Z=1 | cat`, `cd sub | cat`, `unset HOME | cat` and `echo a | exit 3` leave the shell unchanged. `tests/env_builtins.sh`: `export Y=1 &` and `unset K &` leave it unchanged.
- **Changed:** Split across two files. `cd / &` is not tested (D7).

### SEC-T06: history permissions corrected (SEC-006)
- **Now:** No test of the case. `tests/test_history.c` only checks that a save resets a 0644 file to 0600.
- **Changed:** Not covered. The shell doesn't fix the mode at load or print a warning (E6, E17).

### SEC-T07: no descriptor leakage (SEC-007)
- **Now:** `tests/sec_t07.sh`: a child counts its `/dev/fd` entries with and without a history file; the counts must match. `tests/script.sh`: a script's own fd is not passed to commands.
- **Changed:** Not interactive, and it compares counts instead of listing `/proc/self/fd` (E16). It relies on history being used with piped stdin (C3).

## 2. Added during the sprints

Automated tests that exist but match no Test Plan case.

| File / target | What it checks | Requirement |
|---|---|---|
| `tests/negative.sh` (cases beyond the NEG-01 examples) | Other syntax errors: `cat <`, `echo hi >>`, `ls 2>`, `&`, `ls > > f`, `echo a & echo b`, `ls &>`, `ls & \|` (status 2). | NFR-004, NFR-007, FR-002, FR-006 |
| `tests/negative.sh` | `./adir` (126) and `sh -c 'kill -9 $$'` (137, killed by signal). | FR-004, NFR-007 |
| `tests/negative.sh` | Built-in errors with exact text: `export 1A`, `export 1A=5`, `export =5`, `unset 9x`, `env extra`, `cd a b`, `exit 1 2`, `bg %9`, `fg`, `unalias nosuch`. | FR-009, FR-011, NFR-007 |
| `tests/negative.sh` | Redirection errors: `cat < /no/such/file`, `echo x > /no/such/dir/f` (status 1). | FR-006, NFR-007 |
| `tests/negative.sh` | Pipeline errors: `ls \|`, `ls \| \| cat`, `> f \| cat`, 17 stages (status 2); `echo a \| nosuchcmd_xyz` (127); `echo hi \| ./noexec` (126); `cat < /no/such/file \| cat` (0). | FR-005, NFR-007 |
| `tests/negative.sh` | Odd bytes: a quoted word starting with the `\x1f` marker (status 2, "invalid control character"); empty command name `''` (127); control-character command name (127). | SEC-002, FR-004 |
| `tests/lexer.sh` | A quoted word or `$M` value starting with the internal marker byte is refused (status 2) and does not redirect. Quoted words reach argv intact. | SEC-002, FR-002 |
| `tests/test_lexer.c` | All operators, double-quote escapes, empty quotes, backslash-newline, a 64 KiB token, the argv adapter and the marker-byte check. | FR-002, SEC-001, SEC-002 |
| `tests/test_expander.c` | No word splitting, quoted empty expansion, escapes, name boundaries (`${HOME}`, `$1`, `$-`, `$`), values not re-expanded, redirect targets, a 140 KB value. | FR-003, SEC-001, SEC-002 |
| `tests/expander.sh` | `$NAME`, `$UNDEF`, quoting and `$?` in the running shell; a value with two spaces stays one argument; a `$W` value with `;`, `>` and `\|` is not acted on. | FR-003, SEC-002 |
| `tests/test_env.c` | `export`/`unset`/`env`: name validation, values with `=` and spaces, `export NAME`, invalid names (status 1), `env` with arguments (status 2). | FR-009 |
| `tests/env_builtins.sh` | Exported variables reach a child (`printenv`); `export 'S=a b=c'`; exact error text for `export 1A` and `unset 9x`. | FR-009, NFR-007 |
| `tests/test_error.c` | `sh_error()` output format `simple_shell: <context>: <message>` and the status constants 1, 2, 126, 127, 128. | NFR-007 |
| `tests/test_prompt.c` | Prompt forms (outside home, prefix of home, `HOME=/`, no home, lookup failure → `$ `); blank-line detection; `privileges_elevated()`. | FR-001, BR-5, SEC-004 |
| `tests/repl.sh` | EOF exits with the last status (3). Blank and whitespace lines don't change `$?` or reach history. The shell started via `sh -c` in a pty is not stopped by SIGTTOU. | FR-001, BR-5, FR-010 |
| `tests/test_builtins.c` | `pwd`, `echo -n` and `exit 3` (sets `running = 0`, status 3). | FR-009 |
| `tests/test_job_control.c` | `jobs`/`fg`/`bg` are job-control built-ins; a reaped job leaves the table. | FR-009, FR-007 |
| `tests/test_pipeline.c` | `pipeline_join()` round trip; a stage with only a redirection is invalid. | FR-005 |
| `tests/pipeline.sh` | Quoted `\|` is a plain character; `yes \| head -n 1` ends (SIGPIPE); redirections per stage; aliases in every stage and an alias holding a pipe; a background pipeline; pipe syntax errors with prefixed messages; a missing stage command gives 127. | FR-005, FR-006, FR-007, FR-011, NFR-007 |
| `tests/script.sh` | `#` comments and `#!` line skipped; `cat` in a script reads the real stdin; errors for missing (127), directory (126), unreadable (126) script and extra arguments (2); the script keeps going after a failing line. | FR-012 (D9, D10) |
| `tests/script.sh` fd block | The script file's descriptor is not passed to commands. | SEC-007 |
| `tests/redirection.sh` | Shell stdout is restored after a redirect; `echo attached>file` with no space. | FR-006 |
| `make check-injection` (and its self-test in `tests/sec_t02.sh`) | No `system(`, `popen(` or `/bin/sh` in `src` and `include`. The self-test proves it catches each one and doesn't flag `filesystem(`. Runs as a CI step. | SEC-002 |
| `make fuzz` / `tests/fuzz.c lex` | In-process fuzz of lex → expand → argv with random `X`, `HOME` and `$?` values, under ASan/UBSan. (Part of SEC-T01; the in-process half is new.) | SEC-001 |
| `make asan` | The MEM-01 input under ASan/UBSan with leak detection (in the Dockerfile). | NFR-002 |
| `make check-globals` | No writable data symbol in any object. Not in CI; currently fails (E14, F20). | NFR-005 |
| `tests/test_dispatch.c` | Dispatch overhead. Listed under PT-01 above (D11 says it implements PT-01). | NFR-001 |
