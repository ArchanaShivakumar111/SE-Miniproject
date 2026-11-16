# System Validation Report

Project SH-2026-10, Team 10. Each of the 36 test cases in Test Plan v1.0 Section 14, in Test Plan order, with the SRS requirement it validates, the automated test that checks it, how much of the case that test covers, and the result on the frozen code.

- **Code:** tag `code-freeze`, commit `907af3f` (16 Nov 2026).
- **Run:** 17 Nov 2026, 00:38 IST, Ubuntu 24.04.4 on WSL 2, gcc 13.3.0, normal user. Details in `docs/Unit_Test_Report.md`.
- **CI:** the Docker build on Ubuntu 22.04 and Debian 12 runs `make`, `make lint`, `make test`, `make valgrind` and `make asan` as root. `.github/workflows/ci.yml` also runs `make check-injection` and `make fuzz`.
- **Deviations:** row IDs refer to `docs/DEVIATIONS.md`.

**Coverage** says how much of the case's expected result the test checks:
- **Full:** every expected result is checked automatically. The input or form may differ; the DEVIATIONS row explains how.
- **Partial:** some expected results are checked automatically, others aren't.
- **Manual:** no automated test; run by hand on a pseudo-terminal.
- **Not covered:** no test of the case's expected result.

**Result** is what happened in the run above. **Pass** means every check mapped to the case passed.

| Test ID | Title (short) | SRS requirement(s) | Automated by | Coverage | Result | Deviation |
|---|---|---|---|---|---|---|
| UT-LEX-01 | Quotes and escapes | FR-002 | `tests/test_lexer.c` `test_quotes_and_escapes()`: same input, 4 WORD tokens, plus qmask checks | Full | Pass | — |
| UT-LEX-02 | Unterminated quote | FR-002, NFR-004 | `tests/test_lexer.c` `test_unterminated()`: `lex("echo \"abc")` returns NULL, `ERR_SYNTAX`. Also `tests/lexer.sh` (status 2, exact message, nothing runs) | Full | Pass | — |
| UT-LEX-03 | Operators without spaces | FR-002 | `tests/test_lexer.c` `test_operators_no_spaces()`: `ls>o 2>e\|wc&` gives the 8 expected tokens | Full | Pass | — |
| UT-EXP-01 | Variable expansion | FR-003 | `tests/test_expander.c` first `EXPECT`: `echo $HOME $UNDEF '$HOME'` gives `[echo, /home/t, $HOME]` | Full | Pass | — |
| UT-EXP-02 | `$?` expansion | FR-003 | `tests/test_expander.c`: `last_status = 1`, `echo $?` gives `[echo, 1]` | Full | Pass | — |
| UT-PAR-01 | Parse pipeline with redirections | FR-005, FR-006 | `tests/test_pipeline.c` (stage split, 16-stage limit, empty stage); `tests/test_lexer.c` `test_argv_adapter()` (`cat <in >>out 'x y' &` gives marker argv). No `parse()`/`Command` fields to check | Partial | Pass | C2, F1 |
| IT-EXE-01 | Run an external command | FR-004 | `tests/expander.sh` (`false; echo $?` = 1, `true; echo "status $?"` = 0); `tests/sec_t02.sh` (external `printf` output exact); `tests/pipeline.sh` (external `tr`, `sort`, `head`). `ls /` is not run or compared with bash | Full | Pass | — |
| IT-EXE-02 | Unknown / non-executable command | FR-004, NFR-007 | `tests/negative.sh` `nosuchcmd_xyz` (127, prefixed message), `./noexec` (126), shell keeps running; `tests/expander.sh` (`$?` = 127). Message text is "No such file or directory", not "command not found" | Partial | Pass (statuses and prefix); message text differs | D1 |
| IT-PIP-01 | Multi-stage and 16-stage pipelines | FR-005 | `tests/pipeline.sh`: 2, 4 and 16 stages, last-stage `$?`, no leftover children. Not run: `seq 1 100000 \| grep 7 \| sort -n \| tail -1`, the < 1 s timing, `/proc/<pid>/fd` listing | Partial | Pass | D6 |
| IT-RED-01 | All redirection operators | FR-006 | `tests/redirection.sh`: `>`, `>>`, `<`, `2>`, `&>`, no space before `>`, nothing else on stdout. (Interactive `cat < file` failed before PR #59 fixed it; no test runs `<` on a pty) | Full | Pass | — |
| IT-RED-02 | Redirect to unwritable file | FR-006, SEC-003 | `tests/sec_t03.sh` first part: write to a mode 444 file fails, file unchanged, command not run, stderr not empty. Exact message and `$?` = 1 not checked | Partial | Pass | F17 |
| IT-JOB-01 | Background job | FR-007 | `tests/background_jobs.sh` (`[1] pid`, `[2] pid`, not blocked, < 2.5 s); `tests/pipeline.sh` (`jobs` shows `[1] Running  sleep 2 \| cat &`). No `Done` message, no `+`, no < 100 ms check | Partial | Pass | F6, F13, F18 |
| ST-SIG-01 | Ctrl+C reaches only the child | FR-008 | None automated. Run by hand on a pty: `sleep 30`, Ctrl+C gives status 130 and the shell keeps running | Manual | Pass (manual) | F18 |
| ST-SIG-02 | Stop and resume a job | FR-008, FR-007, FR-009 | `tests/test_job_control.c`: `jobs` lists a SIGSTOP'd child as `[1] Stopped`, `bg %1` sets Running, `fg` resumes and waits (status 42). Ctrl+Z and Ctrl+C run by hand on a pty: `[1] Stopped sleep 60`, `bg`, `jobs` (`Running`), `fg`, Ctrl+C (130) | Partial | Pass (automated and manual) | F10, F18 |
| UT-BI-01 | `cd` variants | FR-009 | `tests/test_builtins.c`: `cd /tmp` (0), `cd /no/such/dir` (1), `cd a b` (1). No `cd` with no argument, `cd -` or `cd ~`; cwd-unchanged not checked | Partial | Pass (`cd -` and `cd ~` not supported) | E1, E2, E17 |
| IT-BI-02 | Built-ins run in parent | FR-009 | `tests/expander.sh` IT-BI-02 block (`export X=5`, `unset X`, `echo [$X]` = `[]`, `cd /tmp; pwd`, `exit 3` exits 3); `tests/env_builtins.sh` (`env > file` has `X=5`, then no `X` after `unset`) | Full | Pass | — |
| UT-HIS-01 | Ring buffer boundary | FR-010 | `tests/test_history.c`: 1,500 adds, count = 1,000, first `cmd500`, last `cmd1499` | Full | Pass | — |
| IT-HIS-02 | History persists across sessions | FR-010, SEC-006 | `tests/test_history.c`: save leaves mode 0600, load restores the entries; `tests/repl.sh` on a pty: an interactive session writes its lines, without blank ones, to `~/.simple_shell_history`. No `history` built-in to list them | Partial | Pass (listing not possible) | E5, E17 |
| UT-ALI-01 | Alias set, expand, guard, remove | FR-011 | `tests/test_alias.c`: `ll` expands, self-referring `ls` expands once, `a`/`b` loop stops, `alias_remove("ll")` then `alias_get` = NULL | Full | Pass | — |
| ST-REPL-01 | Prompt format and long line | FR-001 | `tests/repl.sh`: 64 KiB echo line via piped stdin, no prompt when not a TTY; pty run via `script` checks `user@host:~/w$ `, `~$ `, `/$ `. `tests/test_prompt.c` checks `tester@box:~/w$ ` | Full | Pass | B7 |
| ST-BAT-01 | Script mode | FR-012 | `tests/script.sh`: output with no prompt, `exit 7` status, last-command status, piped `echo piped`; neither a script nor piped stdin creates or changes the history file | Full | Pass | C3 (resolved), D10 |
| ST-E2E-01 | Realistic end-to-end session | FR-001..FR-011 | None as one session. Its parts are tested in other files | Not covered | Not run. As written it would fail at `cd ~/e2e` and `history` | E2, E5, F13 |
| PT-01 | Dispatch overhead | NFR-001 | `tests/test_dispatch.c`: in-process `shell_execute_line("true")` vs bare fork/exec, 200 runs, best of 5, limit 2 ms | Partial | Pass (−0.047 ms per command) | D11 |
| PT-02 | Start-up with full history | NFR-008 | `tests/perf_pt02.sh` on a pty: extra start-up time from loading a 1,000-entry history (run with history minus run without), best of 5, limit 100 ms; checks the history was really loaded | Partial | Pass (7 ms) | E15 |
| MEM-01 | Leak check | NFR-002 | `make valgrind` and `make asan` (in the Dockerfile, so in CI): 1,000 `echo N > /dev/null` lines plus alias/unalias | Partial | Pass (0 errors, 0 bytes lost) | E12 |
| ZMB-01 | Zombie reaping | NFR-003 | `tests/zombie_processes.sh`: one `sleep 0.5 &`, state read from `/proc` after 1 s; `tests/pipeline.sh` last block (no leftover pipeline children) | Partial | Pass | F16 |
| NEG-01 | Negative suite (≥ 30 inputs) | NFR-004, NFR-007 | `tests/negative.sh`: 42 inline cases; status, empty stdout, `simple_shell: ` prefix, next line `echo alive` still runs. Includes bad quotes, `\| ls`, `ls >`, missing files, 100 KB line, `fg %9` | Full | Pass (42 of 42) | C4, D12 |
| QA-01 | Modularity and static analysis | NFR-005 | `make lint` (Cppcheck, in CI); `make check-globals` (not in `make test` or CI) | Partial | Lint pass; `check-globals` fails (`table`, `child_event_pending`) | E13, E14, F20 |
| PORT-01 | Build matrix | NFR-006 | `.github/workflows/ci.yml` matrix Ubuntu 22.04 / Debian 12: the Docker build runs `make` (`-Werror`) and `make test` | Full | Pass (CI on PR #61, the freeze commit, both images) | F19 |
| SEC-T01 | Fuzzing with ASan/UBSan | SEC-001 | `make fuzz` (in CI): `tests/fuzz.c lex 1000` in-process, and `tests/fuzz_shell.sh` 1,000 lines up to 1 MB through the sanitizer build | Full | Pass (0 sanitizer reports) | B16 |
| SEC-T02 | Command injection | SEC-002 | `tests/sec_t02.sh` (`echo "x; rm /tmp/canary"`, quoted metacharacters, `make check-injection` and its self-test); `tests/expander.sh` (`export V='$(rm /tmp/canary)'; echo $V`, canary survives); `make check-injection` step in CI | Full | Pass | — |
| SEC-T03 | Permission bypass | SEC-003 | `tests/sec_t03.sh`: write to a 444 file and read a 000 file both fail (as `nobody` under root). Own files, not `/home/other/secret`; checks a non-zero status, not `$?` = 1 or the exact message | Partial | Pass | F17 |
| SEC-T04 | Refuse setuid | SEC-004 | `tests/sec_t04.sh`: setuid (4755) and setgid (2755) copies owned by `nobody` exit 1 with the exact message (root only, so in the Docker build); `tests/test_prompt.c` `privileges_elevated()` cases. Skips when not root | Full | Pass (setuid run in CI as root; skipped in the local run) | B9 |
| SEC-T05 | Environment isolation | SEC-005 | `tests/pipeline.sh` (`export Z=1 \| cat`, `cd sub \| cat`, `unset HOME \| cat`, `echo a \| exit 3`); `tests/env_builtins.sh` (`export Y=1 &`, `unset K &`). `cd / &` not run | Partial | Pass | D7 |
| SEC-T06 | History permissions corrected | SEC-006 | None. The shell doesn't fix the mode or warn at load. `tests/test_history.c` only checks that a save resets 0644 to 0600 | Not covered | Not met: feature not implemented | E6, E17 |
| SEC-T07 | No descriptor leakage | SEC-007 | `tests/sec_t07.sh` on a pty: `/dev/fd` count in a child is the same with and without a history file, and the history file was loaded and saved. `tests/script.sh` fd block: the script's fd isn't passed on. Absolute "only 0, 1, 2" and close-on-exec itself not checked | Partial | Pass | E16 |

## Totals

| Coverage | Count |
|---|---|
| Full | 17 |
| Partial | 16 |
| Manual | 1 |
| Not covered | 2 |
| **Total** | **36** |

| Result | Count |
|---|---|
| Pass | 33 (including ST-SIG-01, run by hand) |
| Partly passed (QA-01: lint passes, `check-globals` fails) | 1 |
| Not run (ST-E2E-01, no test) | 1 |
| Not met (SEC-T06, feature not implemented) | 1 |
| **Total** | **36** |

A pass on a **Partial** case means the parts that are tested passed. The parts that aren't, and why, are in the DEVIATIONS rows.
