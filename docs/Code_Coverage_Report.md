# Code Coverage Report (line and function coverage)

Project SH-2026-10, Team 10. Line and function coverage of `src/` over the whole `make test` run, on the frozen code (tag `code-freeze`, commit `907af3f`). Branch coverage is in `docs/Branch_Coverage_Report.md`.

## Result

| Metric | Covered | Total | Coverage |
|---|---|---|---|
| Lines | 1,353 | 1,741 | **77.7%** |
| Functions | 115 | 117 | **98.3%** |
| Branches | 833 | 1,170 | 71.2% (see the branch report) |

Run on 17 Nov 2026 (00:38 IST) on Ubuntu 24.04.4 (WSL 2), gcc 13.3.0, gcovr 7.0, as a normal user. All 26 test suites passed in the instrumented build.

## How it was measured

```
make coverage
```

- Builds `src/` and the tests with `gcc --coverage` (same `-std=c11 -Wall -Wextra -Werror -pedantic` flags), then runs the whole `make test`: the 11 C unit-test programs and the 15 shell-level scripts, so the count includes the shell as users run it, not only the unit tests.
- PT-02 (start-up time) is skipped in this build only, because instrumented code is slower; the normal `make test` still runs it.
- Needs **gcovr** (`sudo apt install gcovr`). Reports are written to `docs/coverage/`: `line_coverage.txt`, `branch_coverage.txt` and `index.html` (per-file pages with each line marked).
- Only `src/` is measured; test code is excluded.

### Code that runs in forked children

Pipeline stages, redirections and built-ins in a pipeline or in the background run in forked children that end with `_exit()` or `execvp()`. Both skip gcov's normal write at exit, so with plain `--coverage` every line run only in a child is reported as never run. For the coverage build only, `make coverage` links the shell and tests with `ld --wrap=fork,--wrap=_exit,--wrap=execvp` and a small helper the Makefile generates (`build/cov/gcov_flush.c`): a new child starts with zeroed counters, and writes them before `_exit` or `exec`. Normal builds don't use it; `make` produces the same binary as without it.

The effect on the two files that fork:

| File | Without the helper | With the helper |
|---|---|---|
| `src/pipeline.c` | 46.4% | **81.2%** |
| `src/shell.c` | 56.9% | **66.1%** |

(Measured with `gcov` on the same test run, 16–17 Nov.)

## By file

| File | Lines | Covered | Line coverage |
|---|---|---|---|
| `src/alias.c` | 88 | 80 | 90% |
| `src/env.c` | 67 | 59 | 88% |
| `src/lexer.c` | 195 | 171 | 87% |
| `src/error.c` | 27 | 23 | 85% |
| `src/repl.c` | 60 | 51 | 85% |
| `src/expander.c` | 108 | 91 | 84% |
| `src/history.c` | 72 | 61 | 84% |
| `src/pipeline.c` | 181 | 147 | 81% |
| `src/jobs.c` | 126 | 101 | 80% |
| `src/main.c` | 180 | 136 | 75% |
| `src/redirection.c` | 148 | 108 | 73% |
| `src/builtins.c` | 215 | 144 | 67% |
| `src/shell.c` | 274 | 181 | 66% |
| **Total** | **1,741** | **1,353** | **77.7%** |

The line numbers not run are listed in `docs/coverage/line_coverage.txt`.

## What isn't covered, and why

Most of the 388 lines not run fall into four groups:

1. **Interactive terminal and job-control paths** (`shell.c`, `builtins.c`, `jobs.c`, `main.c`). Giving the terminal to a job (`tcsetpgrp`), handling a job stopped with Ctrl+Z, and `fg`/`bg` on a terminal run only in an interactive shell. The automated suites mostly use piped input. These paths were run by hand on a pseudo-terminal (see `docs/Unit_Test_Report.md`), but that run isn't counted here (DEVIATIONS F18).
2. **System-call failure paths**. Code that handles `fork`, `pipe`, `dup2`, `waitpid`, `open` or memory allocation failing, for example killing started stages when a later `fork` fails. These can't be triggered without fault injection, which the project doesn't use.
3. **Built-in forms with no test**. Listing aliases (`alias` with no argument, `alias name`), and some `fg`/`bg` error messages (DEVIATIONS E17).
4. **Root-only code**. The SEC-T04 refusal of a setuid binary (`main.c`) needs root; this run was as a normal user. CI runs that test as root.

The 2 functions never called:

- `wait_for_child()` (`shell.c:20`) is called only on failure paths: when the terminal can't be given to a job, when `setpgid` fails, or when a background job can't be added to the job table (group 2).
- `job_table_print()` (`jobs.c:202`) is declared in `jobs.h` but nothing calls it; the `jobs` built-in uses `job_table_print_status()`. It is unused code, not a missing test.

SRS v1.1 sets no coverage target, so this report records the measured values without a pass/fail threshold.
