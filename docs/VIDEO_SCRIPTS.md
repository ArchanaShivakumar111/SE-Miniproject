# Sprint video scripts

Exact commands for the two outcome videos. Record them from the frozen `main`, in a Linux or WSL terminal, after running `make`. Start the shell with `./build/simple_shell`; every command below is typed at its prompt unless it says "(host shell)". Put the real recording date in the video's file name or caption.

Tip: before recording, run `cd /tmp && mkdir -p demo && cd demo` in the host shell so the files the demo creates stay out of the repo.

## Sprint 1 video (1 minute)

Covers issues #1–#16: prompt, lexer, external commands, pipelines, redirection, built-ins, history, error format, CI.

| Time | Type | What it shows |
|---|---|---|
| 0:00 | `./build/simple_shell` (host shell) | Prompt `user@host:cwd$` (FR-001) |
| 0:05 | `pwd` then `ls` | Built-in and external command via PATH (FR-009a, FR-004) |
| 0:12 | `echo "hello   world" 'single $HOME' \$HOME` | Quotes and escapes keep spaces and `$` literal (FR-002) |
| 0:20 | `echo first > out.txt`, `echo second >> out.txt`, `cat < out.txt` | `>`, `>>`, `<` (FR-006) |
| 0:28 | `ls /nonexistent 2> err.txt`, then `cat err.txt` | `2>` redirection (FR-006) |
| 0:34 | `echo pipes are working \| tr a-z A-Z \| rev` | 3-stage pipeline (FR-005) |
| 0:40 | `export GREETING=hi`, `env \| grep GREETING`, `unset GREETING` | export/env/unset (FR-009b) |
| 0:47 | `nosuchcommand`, then `cd a b` | Errors on stderr as `simple_shell: ...` (NFR-007) |
| 0:52 | `exit`, then `ls -l ~/.simple_shell_history` (host shell) | History saved with mode `-rw-------` (FR-010, SEC-006) |
| 0:56 | Browser: the repo's Actions tab, a green run on `main` | CI on Ubuntu and Debian (NFR-006) |

Expected output checked on 12 Nov: the pipeline prints `GNIKROW ERA SEPIP`; `cat < out.txt` prints `first` and `second`; `cd a b` prints `simple_shell: cd: too many arguments`.

## Sprint 2 video (2 minutes)

Covers issues #17–#30: expansion, background jobs, signals, jobs/fg/bg, aliases, script mode, SEC-005, and the test and coverage results.

| Time | Type | What it shows |
|---|---|---|
| 0:00 | `export NAME=Team10`, `echo "hello $NAME"` | `$VAR` expansion (FR-003) |
| 0:08 | `false`, then `echo $?` | `$?` prints `1` (FR-003) |
| 0:14 | `alias ll='ls -l'`, `ll`, `unalias ll` | Aliases (FR-011) |
| 0:24 | `sleep 30 &`, then `jobs` | Background job and job table (FR-007, FR-009c) |
| 0:34 | `sleep 60`, press Ctrl+Z, `jobs` | Ctrl+Z stops only the foreground job (FR-008) |
| 0:44 | `bg`, `jobs`, then `fg`, press Ctrl+C | Job moves to background and back; Ctrl+C ends it, the shell keeps running (FR-008, FR-009c) |
| 0:58 | `export Z=1 \| cat`, then `env \| grep -c '^Z='` | Prints `0`: a built-in in a pipeline doesn't change the shell (SEC-005) |
| 1:06 | `exit`; `printf 'echo from a script: $HOME\n' > s.sh`; `./build/simple_shell s.sh` (host shell) | Script mode (FR-012) |
| 1:16 | `make test` (host shell, in the repo) | All suites pass, including the 42-case negative suite (NFR-004) |
| 1:36 | `make coverage` (host shell), show the summary or `docs/coverage/index.html` | Line and branch coverage reports |
| 1:50 | Browser: the GitHub Project board, filtered to Sprint 2 | Every Sprint 2 item Done, with story points |

Expected output checked on 12 Nov: `echo "hello $NAME"` prints `hello Team10`; `echo $?` after `false` prints `1`; `sleep 1 &` prints `[1] <pid>` and `jobs` shows `[1] Running sleep 1 &`; `env | grep -c '^Z='` prints `0`. The Ctrl+Z, `bg`, `fg` and Ctrl+C steps need a real terminal, so do one practice run first.

Avoid `cd -`: it isn't supported.
