# System Validation Report (draft)

> **Draft, 14 Nov 2026.** The mapping below is complete. The Result column will be added from a real run of every test on the frozen main after the Sprint 2 code freeze; no results are claimed yet.

Project SH-2026-10, Team 10. One row per Test Plan Section 14 case, in Test Plan order (36 cases).
Built from the test files on main at `6d66961`, with DEVIATIONS rows from PR #58. Every mapping was checked by reading the test file.

Coverage values:
- **Full**: every expected result of the case is checked by an automated test. The input or form may differ; any DEVIATIONS row that explains the difference is cited.
- **Partial**: some expected results are checked automatically, others are not.
- **Manual**: no automated test; needs a pty or a manual run.
- **Not covered**: no automated test of the case's expected result.

`make test` runs every `tests/test_*.c` and the listed `tests/*.sh`. The Dockerfile runs `make`, `make lint`, `make test`, `make valgrind` and `make asan` as root. CI (`.github/workflows/ci.yml`) runs `make check-injection`, `make fuzz` and that Docker build on Ubuntu 22.04 and Debian 12.

| Test ID | Title (short) | SRS requirement(s) | Automated by (file / make target, with the specific check if a file holds many) | Coverage | Deviation |
|---|---|---|---|---|---|
| UT-LEX-01 | Quotes and escapes | FR-002 | `tests/test_lexer.c` `test_quotes_and_escapes()`: same input, 4 WORD tokens, plus qmask checks | Full | — |
| UT-LEX-02 | Unterminated quote | FR-002, NFR-004 | `tests/test_lexer.c` `test_unterminated()`: `lex("echo \"abc")` returns NULL, `ERR_SYNTAX`. Also `tests/lexer.sh` (status 2, exact message, nothing runs) | Full | — |
| UT-LEX-03 | Operators without spaces | FR-002 | `tests/test_lexer.c` `test_operators_no_spaces()`: `ls>o 2>e\|wc&` gives the 8 expected tokens | Full | — |
| UT-EXP-01 | Variable expansion | FR-003 | `tests/test_expander.c` first `EXPECT`: `echo $HOME $UNDEF '$HOME'` gives `[echo, /home/t, $HOME]` | Full | — |
| UT-EXP-02 | `$?` expansion | FR-003 | `tests/test_expander.c`: `last_status = 1`, `echo $?` gives `[echo, 1]` | Full | — |
| UT-PAR-01 | Parse pipeline with redirections | FR-005, FR-006 | `tests/test_pipeline.c` (stage split, 16-stage limit, empty stage); `tests/test_lexer.c` `test_argv_adapter()` (`cat <in >>out 'x y' &` gives marker argv). No `parse()`/`Command` fields to check | Partial | C2, F1 |
| IT-EXE-01 | Run an external command | FR-004 | `tests/expander.sh` (`false; echo $?` = 1, `true; echo "status $?"` = 0); `tests/sec_t02.sh` (external `printf` output exact); `tests/pipeline.sh` (external `tr`, `sort`, `head`). `ls /` is not run or diffed against bash | Full | — |
| IT-EXE-02 | Unknown / non-executable command | FR-004, NFR-007 | `tests/negative.sh` `nosuchcmd_xyz` (127, prefixed message), `./noexec` (126), shell keeps running; `tests/expander.sh` (`$?` = 127). Message text is "No such file or directory", not "command not found" | Partial | D1 |
| IT-PIP-01 | Multi-stage and 16-stage pipelines | FR-005 | `tests/pipeline.sh`: 2, 4 and 16 stages, last-stage `$?`, no leftover children. Not run: `seq 1 100000 \| grep 7 \| sort -n \| tail -1`, the < 1 s timing, `/proc/<pid>/fd` listing | Partial | D6 |
| IT-RED-01 | All redirection operators | FR-006 | `tests/redirection.sh`: `>`, `>>`, `<`, `2>`, `&>`, no space before `>`, nothing else on stdout | Full | — |
| IT-RED-02 | Redirect to unwritable file | FR-006, SEC-003 | `tests/sec_t03.sh` first part: write to a mode 444 file fails, file unchanged, command not run, stderr not empty. Exact message and `$?` = 1 not checked | Partial | F17 |
| IT-JOB-01 | Background job | FR-007 | `tests/background_jobs.sh` (`[1] pid`, `[2] pid`, not blocked, < 2.5 s); `tests/pipeline.sh` (`jobs` shows `[1] Running  sleep 2 \| cat &`). No `Done` message, no `+`, no < 100 ms check | Partial | F6, F13, F18 |
| ST-SIG-01 | Ctrl+C reaches only the child | FR-008 | None (no pty/expect test) | Manual | F18 |
| ST-SIG-02 | Stop and resume a job | FR-008, FR-007, FR-009 | `tests/test_job_control.c`: `jobs` lists a SIGSTOP'd child as `[1] Stopped`, `bg %1` sets Running, `fg` resumes and waits (status 42). Ctrl+Z and Ctrl+C keys are not sent | Partial | F10, F18 |
| UT-BI-01 | `cd` variants | FR-009 | `tests/test_builtins.c`: `cd /tmp` (0), `cd /no/such/dir` (1), `cd a b` (1). No `cd` with no argument, `cd -` or `cd ~`; cwd-unchanged not checked | Partial | E1, E2, E17 |
| IT-BI-02 | Built-ins run in parent | FR-009 | `tests/expander.sh` IT-BI-02 block (`export X=5`, `unset X`, `echo [$X]` = `[]`, `cd /tmp; pwd`, `exit 3` exits 3); `tests/env_builtins.sh` (`env > file` has `X=5`, then no `X` after `unset`) | Full | — |
| UT-HIS-01 | Ring buffer boundary | FR-010 | `tests/test_history.c`: 1,500 adds, count = 1,000, first `cmd500`, last `cmd1499` | Full | — |
| IT-HIS-02 | History persists across sessions | FR-010, SEC-006 | `tests/test_history.c`: save creates mode 0600, load restores entries; `tests/repl.sh` (piped session writes `~/.simple_shell_history`). No `history` built-in to list it | Partial | E5, E17, C3 |
| UT-ALI-01 | Alias set, expand, guard, remove | FR-011 | `tests/test_alias.c`: `ll` expands, self-referring `ls` expands once, `a`/`b` loop stops, `alias_remove("ll")` then `alias_get` = NULL | Full | — |
| ST-REPL-01 | Prompt format and long line | FR-001 | `tests/repl.sh`: 64 KiB echo line via piped stdin, no prompt when not a TTY; pty run via `script` checks `user@host:~/w$ `, `~$ `, `/$ `. `tests/test_prompt.c` checks `tester@box:~/w$ ` | Full | B7 |
| ST-BAT-01 | Script mode | FR-012 | `tests/script.sh`: output with no prompt, `exit 7` status, last-command status, piped `echo piped`, script writes no history file. Piped stdin still writes history | Partial | C3 |
| ST-E2E-01 | Realistic end-to-end session | FR-001..FR-011 | None as one session. Parts exist in other files | Not covered | E2, E5, F13 |
| PT-01 | Dispatch overhead | NFR-001 | `tests/test_dispatch.c`: in-process `shell_execute_line("true")` vs bare fork/exec, 200 runs, best of 5, limit 2 ms | Partial | D11 |
| PT-02 | Start-up with full history | NFR-008 | `tests/perf_pt02.sh`: 1,000-entry history, launch + load + exit on EOF, best of 5, limit 100 ms | Partial | E15, C3 |
| MEM-01 | Leak check | NFR-002 | `make valgrind` and `make asan` (run in the Dockerfile, so in CI): 1,000 `echo N > /dev/null` lines plus alias/unalias | Partial | E12 |
| ZMB-01 | Zombie reaping | NFR-003 | `tests/zombie_processes.sh`: one `sleep 0.5 &`, state read from `/proc` after 1 s; `tests/pipeline.sh` last block (no leftover pipeline children) | Partial | F16 |
| NEG-01 | Negative suite (>= 30 inputs) | NFR-004, NFR-007 | `tests/negative.sh`: 42 inline cases; status, empty stdout, `simple_shell: ` prefix, next line `echo alive` still runs. Includes bad quotes, `\| ls`, `ls >`, missing files, 100 KB line, `fg %9` | Full | C4, D12 |
| QA-01 | Modularity and static analysis | NFR-005 | `make lint` (cppcheck, in the Dockerfile, so in CI); `make check-globals` (not in `make test` or CI; currently fails) | Partial | E13, E14, F20 |
| PORT-01 | Build matrix | NFR-006 | `.github/workflows/ci.yml` matrix Ubuntu 22.04 / Debian 12: Docker build runs `make` (`-Werror`) and `make test` | Full | F19 |
| SEC-T01 | Fuzzing with ASan/UBSan | SEC-001 | `make fuzz` (in CI): `tests/fuzz.c lex 1000` in-process, and `tests/fuzz_shell.sh` 1,000 lines up to 1 MB through the sanitizer build | Full | B16 |
| SEC-T02 | Command injection | SEC-002 | `tests/sec_t02.sh` (`echo "x; rm /tmp/canary"`, quoted metacharacters, `make check-injection` and its self-test); `tests/expander.sh` (`export V='$(rm /tmp/canary)'; echo $V`, canary survives); `make check-injection` step in CI | Full | — |
| SEC-T03 | Permission bypass | SEC-003 | `tests/sec_t03.sh`: write to 444 file and read a 000 file both fail (as `nobody` under root). Own files, not `/home/other/secret`; checks non-zero status, not `$?` = 1 or exact message | Partial | F17 |
| SEC-T04 | Refuse setuid | SEC-004 | `tests/sec_t04.sh`: setuid (4755) and setgid (2755) copies owned by `nobody` exit 1 with the exact message (root only, so in the Docker build); `tests/test_prompt.c` `privileges_elevated()` cases. Skips when not root | Full | B9 |
| SEC-T05 | Environment isolation | SEC-005 | `tests/pipeline.sh` (`export Z=1 \| cat`, `cd sub \| cat`, `unset HOME \| cat`, `echo a \| exit 3`); `tests/env_builtins.sh` (`export Y=1 &`, `unset K &`). `cd / &` not run | Partial | D7 |
| SEC-T06 | History permissions corrected | SEC-006 | None. Feature not implemented (no fix or warning at load). `tests/test_history.c` only checks that a save resets 0644 to 0600 | Not covered | E6, E17 |
| SEC-T07 | No descriptor leakage | SEC-007 | `tests/sec_t07.sh`: `/dev/fd` count in a child is the same with and without a history file (piped stdin). `tests/script.sh` fd block: script fd not passed on. No interactive `ls -l /proc/self/fd`; absolute "only 0, 1, 2" not checked | Partial | E16, C3 |

## Totals

| Coverage | Count |
|---|---|
| Full | 16 |
| Partial | 17 |
| Manual | 1 |
| Not covered | 2 |
| **Total** | **36** |
