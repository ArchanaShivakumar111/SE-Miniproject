# Branch Coverage Report

Project SH-2026-10, Team 10. Branch coverage of `src/` over the whole `make test` run, on the frozen code (tag `code-freeze`, commit `907af3f`). Line and function coverage, and how the measurement works, are in `docs/Code_Coverage_Report.md`.

## Result

| Metric | Taken | Total | Coverage |
|---|---|---|---|
| Branches | 833 | 1,170 | **71.2%** |

A branch is one outcome of a decision in the compiled code (each `if`, loop test, `&&`, `||` and `switch` case). It counts as taken when the tests made the program go that way at least once. Same run as the line report: 17 Nov 2026, gcc 13.3.0, gcovr 7.0, `make coverage`, with the child-process handling described there. The full list is in `docs/coverage/branch_coverage.txt`.

## By file

| File | Branches | Taken | Branch coverage |
|---|---|---|---|
| `src/lexer.c` | 138 | 116 | 84% |
| `src/expander.c` | 100 | 84 | 84% |
| `src/repl.c` | 58 | 46 | 79% |
| `src/error.c` | 14 | 11 | 78% |
| `src/alias.c` | 48 | 37 | 77% |
| `src/env.c` | 66 | 51 | 77% |
| `src/history.c` | 40 | 30 | 75% |
| `src/pipeline.c` | 104 | 76 | 73% |
| `src/redirection.c` | 80 | 58 | 72% |
| `src/main.c` | 124 | 84 | 67% |
| `src/shell.c` | 162 | 104 | 64% |
| `src/builtins.c` | 138 | 83 | 60% |
| `src/jobs.c` | 98 | 53 | 54% |
| **Total** | **1,170** | **833** | **71.2%** |

## Reading the numbers

- **Input handling is well covered.** The lexer and expander (84% each) take untrusted input, and are the most tested: unit tests, the 42-case negative suite and 2,000 fuzzed lines (SEC-T01).
- **Branch coverage is lower than line coverage (71% against 78%)** because most error checks are one-line `if (call() < 0)` tests: the line runs every time, but the failure side is almost never taken.
- **`jobs.c` (54%), `builtins.c` (60%) and `shell.c` (64%) are lowest**, for the reasons in the line report: their untaken branches are mostly the interactive side of job control (`if (sh->interactive)`, stopped jobs, `fg`/`bg` on a terminal), which the automated suites don't reach (DEVIATIONS F18), and system-call failure checks. Ctrl+C, Ctrl+Z, `bg` and `fg` were checked by hand on a pseudo-terminal (`docs/Unit_Test_Report.md`), but that run is not counted.
- **`main.c` (67%)** includes the setuid refusal (SEC-T04), which needs root and runs in CI, not in this run.

SRS v1.1 sets no branch-coverage target, so the values are recorded without a pass/fail threshold.
