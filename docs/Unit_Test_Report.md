# Unit Test Report

Project SH-2026-10, Team 10. Results of every automated test and check, run on the frozen code (tag `code-freeze`, commit `907af3f`).

## Test run

| Item | Value |
|---|---|
| Code | `src/`, `include/` and `tests/` identical to tag `code-freeze` (`907af3f`, 16 Nov 2026) |
| Run | 17 Nov 2026, 00:38 IST, by Anshul |
| Machine | Ubuntu 24.04.4 LTS on WSL 2 (kernel 6.6.87.2), normal user (not root) |
| Tools | gcc 13.3.0, GNU make, Valgrind 3.22.0, Cppcheck 2.13.0, gcovr 7.0 |
| Build | `gcc -std=c11 -Wall -Wextra -Werror -pedantic`: 0 warnings |
| Command | `make clean` before each of `make test`, `lint`, `check-globals`, `check-injection`, `valgrind`, `asan`, `fuzz`, `coverage` |

CI runs the same build and `make test` (plus `lint`, `valgrind` and `asan`) as root in Docker on Ubuntu 22.04 and Debian 12; see the CI checks on the pull request that adds this report.

## Summary

| Result | Count |
|---|---|
| Test suites run by `make test` | 26 (11 C unit-test programs, 15 shell-level scripts) |
| Passed | 26 |
| Failed | 0 |
| Partly skipped | 1 (`tests/sec_t04.sh`: the setuid case needs root; see below) |

| Other check | Result |
|---|---|
| `make lint` (Cppcheck) | Pass: 0 findings |
| `make check-injection` (no `system`, `popen`, `/bin/sh`) | Pass |
| `make valgrind` (MEM-01 input) | Pass: 0 errors, 0 bytes in use at exit, "no leaks are possible" |
| `make asan` (same input under ASan/UBSan) | Pass: no reports |
| `make fuzz` | Pass: 1,000 lines through lex/expand in-process, 1,000 lines through the whole shell, 0 sanitizer reports |
| `make check-globals` | **Fail**: `table` (`builtins.c`) and `child_event_pending` (`main.c`). Known and recorded: DEVIATIONS E14, F20. Not part of `make test` or CI |
| `make coverage` | Pass (all 26 suites pass in the instrumented build); see the coverage reports |

## C unit tests (`tests/test_*.c`)

Plain `assert`-based programs (DEVIATIONS B17). Each prints `<name> OK` or aborts on the first failed assertion.

| Program | Module | What it checks | Result |
|---|---|---|---|
| `test_lexer` | `lexer.c` | Quotes and escapes (UT-LEX-01), unterminated quotes (UT-LEX-02), operators without spaces (UT-LEX-03), all operators, 64 KiB token, argv adapter, marker byte | Pass |
| `test_expander` | `expander.c` | `$VAR`, `$?` (UT-EXP-01, -02), no word splitting, escapes, name boundaries, 140 KB value | Pass |
| `test_pipeline` | `pipeline.c` | Stage split, 16-stage limit, empty stage, `pipeline_join()` round trip (part of UT-PAR-01) | Pass |
| `test_builtins` | `builtins.c` | `cd` (UT-BI-01), `pwd`, `echo -n`, `exit 3` | Pass |
| `test_env` | `env.c` | `export`, `unset`, `env`, name validation, error statuses | Pass |
| `test_history` | `history.c` | 1,000-entry ring (UT-HIS-01), save/load, mode 0600 after save, missing file | Pass |
| `test_alias` | `alias.c` | Set, expand, self-reference, loop stop, remove (UT-ALI-01) | Pass |
| `test_job_control` | `jobs.c` | `jobs`/`bg`/`fg` on a SIGSTOP'd child (ST-SIG-02 core), reaping | Pass |
| `test_error` | `error.c` | `simple_shell: <context>: <message>` format, status constants | Pass |
| `test_prompt` | `repl.c` | Prompt forms, blank-line detection, `privileges_elevated()` | Pass |
| `test_dispatch` | `shell.c` | PT-01: dispatch overhead **−0.047 ms** per command (limit 2.0 ms) | Pass |

## Shell-level suites (`tests/*.sh`)

Each runs the built `build/simple_shell` with piped input, script files or a pseudo-terminal (`script`).

| Script | Covers | Result |
|---|---|---|
| `lexer.sh` | Unterminated quote at shell level, marker-byte refusal | Pass |
| `expander.sh` | `$VAR`/`$?` in the shell, IT-BI-02, injection through `$V` | Pass |
| `redirection.sh` | IT-RED-01: `>`, `>>`, `<`, `2>`, `&>` | Pass |
| `pipeline.sh` | IT-PIP-01 (2, 4, 16 stages), SEC-T05, aliases in stages, background pipeline | Pass |
| `env_builtins.sh` | `export`/`unset`/`env`, background built-ins | Pass |
| `background_jobs.sh` | IT-JOB-01: `[1] pid`, `[2] pid`, prompt not blocked | Pass |
| `zombie_processes.sh` | ZMB-01: no zombie after a background job | Pass |
| `repl.sh` | ST-REPL-01 (64 KiB line, prompt on a pty), EOF status, blank lines kept out of history (pty) | Pass |
| `script.sh` | ST-BAT-01: script mode, piped stdin writes no history, script fd not inherited, script errors | Pass |
| `negative.sh` | NEG-01: **42** malformed inputs, each with an error and the shell still running | Pass |
| `sec_t02.sh` | SEC-T02: command injection, `check-injection` self-test | Pass |
| `sec_t03.sh` | SEC-T03 / IT-RED-02: unwritable and unreadable redirect targets | Pass |
| `sec_t04.sh` | SEC-T04: refuse setuid/setgid | Pass (setuid case **skipped**: needs root; it runs in CI as root, and `test_prompt` covers the check) |
| `sec_t07.sh` | SEC-T07: descriptor count with and without a history file, on a pty | Pass |
| `perf_pt02.sh` | PT-02: loading 1,000 history entries adds **7 ms** to start-up (limit 100 ms), on a pty | Pass |

## Manual run: Ctrl+C and Ctrl+Z

ST-SIG-01 and the key presses in ST-SIG-02 have no automated test (DEVIATIONS F18). They were run once on a pseudo-terminal with Python's `pty` module, sending the real Ctrl+C (`\x03`) and Ctrl+Z (`\x1a`) bytes:

| Step | Output | Result |
|---|---|---|
| `sleep 30`, Ctrl+C, `echo $?` | `130`; the shell shows the next prompt | Pass (ST-SIG-01) |
| `sleep 60`, Ctrl+Z | `[1] Stopped sleep 60` | Pass (no `+`, F10) |
| `jobs` / `bg` / `jobs` | `[1] Stopped sleep 60` / `[1] sleep 60 &` / `[1] Running sleep 60` | Pass |
| `fg`, Ctrl+C, `echo $?` | `130` | Pass (ST-SIG-02) |
| `echo ALIVE`, `exit` | `ALIVE`; shell exits 0 | Pass |

## Known gaps

- `make check-globals` fails (E14, F20). It is stricter than QA-01 and is not run by CI.
- The setuid case of SEC-T04 runs only as root, so it was skipped here and runs in CI.
- PR #59 fixed `cat < file` in an interactive session ("Inappropriate ioctl for device"). No automated test covers it; it was checked by hand on a pty before the merge.
- Test Plan cases without a full automated test are listed in `docs/System_Validation_Report.md`.
