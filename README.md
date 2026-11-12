# Simple Shell (SH-2026-10)

A POSIX command-line shell written in C11, built by Team 10 for UE24CS341A (Software Engineering), PES University. It runs commands interactively or from a script, with pipelines, redirection, variables, job control, history and aliases.

## Build and run

Needs Linux (or WSL) with `gcc` and `make`.

```sh
make                      # builds build/simple_shell (gcc -std=c11 -Wall -Wextra -Werror -pedantic)
./build/simple_shell      # interactive: prompt is user@host:cwd$
./build/simple_shell script.sh   # script mode
```

Or with Docker (builds and runs all checks on Ubuntu 22.04 by default):

```sh
docker build -t simple-shell . && docker run -it simple-shell
```

## Features

| Area | What it supports | Requirement |
|---|---|---|
| Prompt and input | `user@host:cwd$` prompt, lines of any length, Ctrl+D to exit | FR-001 |
| Lexer | Single and double quotes, backslash escapes, operators | FR-002 |
| Expansion | `$NAME` and `$?`, no word splitting | FR-003 |
| Commands | External commands through `PATH` (`fork`/`execvp`) | FR-004 |
| Pipelines | 2 to 16 stages | FR-005 |
| Redirection | `<`, `>`, `>>`, `2>`, `&>` | FR-006 |
| Jobs | `&`, Ctrl+C / Ctrl+Z to the foreground job only, `jobs`, `fg`, `bg` | FR-007, FR-008, FR-009c |
| Built-ins | `cd`, `pwd`, `echo`, `exit`, `export`, `unset`, `env`, `alias`, `unalias`, `jobs`, `fg`, `bg` | FR-009a, FR-009b, FR-011 |
| History | 1,000-entry ring buffer saved to `~/.simple_shell_history` (mode 0600) | FR-010, SEC-006 |
| Script mode | `simple_shell file.sh` runs a file non-interactively | FR-012 |
| Security | No `system()`/`popen()`, refuses to run setuid/setgid, `O_CLOEXEC` on files, redirection respects permissions | SEC-001 to SEC-007 |

Errors go to stderr as `simple_shell: <context>: <message>` (NFR-007).

## Tests and checks

| Command | What it checks |
|---|---|
| `make test` | All unit tests (`tests/test_*.c`) and shell-level suites (`tests/*.sh`) |
| `make coverage` | Line and branch coverage of `src/` over `make test`, written to `docs/coverage/` (needs `gcovr`) |
| `make lint` | Cppcheck static analysis, 0 findings (NFR-005) |
| `make check-globals` | No mutable globals in any module (NFR-005) |
| `make valgrind` / `make asan` | 1,000 commands with no leaks or invalid accesses (NFR-002) |
| `make fuzz` | ASan/UBSan fuzzing of the lexer, expander and whole shell (SEC-001) |
| `make check-injection` | No `system()`, `popen()` or `/bin/sh` in the sources (SEC-002) |

GitHub Actions runs these on Ubuntu 22.04 and Debian 12 for every push and pull request (`.github/workflows/ci.yml`, NFR-006).

## Documents

- SRS, SAD and Test Plan (Part-1 submission): [`Team10_Combined.pdf`](Team10_Combined.pdf)
- Deviations from the SRS/SAD/Test Plan: [`docs/DEVIATIONS.md`](docs/DEVIATIONS.md)
- Coverage reports: [`docs/coverage/`](docs/coverage/)
- Sprint video scripts: [`docs/VIDEO_SCRIPTS.md`](docs/VIDEO_SCRIPTS.md)
- Backlog, story points and sprints: the "Simple Shell Backlog" GitHub Project linked to this repository

## Team

| Member | GitHub | Main areas |
|---|---|---|
| Archana Shivakumar (repo owner) | ArchanaShivakumar111 | Setup, command execution, pipelines, script mode, dispatch and negative tests, SEC-005 |
| Anshul Poovaiah K | Ansh7-7 | Prompt and input loop, lexer, expansion, export/unset/env, error format, SEC-001/002/004 |
| Ashlesh R V | ashlesh-rv | CI and Docker, redirection, background jobs, signals, jobs/fg/bg, zombie reaping, SEC-003 |
| Atishay | AtishayJain8046 | cd/pwd/echo/exit, history, aliases, memory checks, modularity, startup time, SEC-006/007 |
