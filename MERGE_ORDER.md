# Merge order for Anshul's PRs (Sprint 1 and Sprint 2)

PR #41 removed pipelines (FR-005) from main. My #9 and #11 pipeline tests are pending until Archana restores it.

## Order

The PRs are stacked: each branch starts from the one before it, because every PR adds a line at the same place in the `Makefile` test list. Please merge them **in this order, using merge commits (not squash)**. After each merge, GitHub shows only the next PR's own commits.

| # | PR | Issue | Branch | Based on | Notes |
|---|---|---|---|---|---|
| 1 | #42 | #4 FR-002 lexer | `anshul/fr-002` | main | `lex()`/`TokenList`/qmask. `lexer_tokenize()` is kept as an adapter. |
| 2 | #43 | #3 FR-001 prompt and input loop | `anshul/fr-001` | #42 | Also fixes a start-up hang: SIGTTOU is now ignored before `tcsetpgrp()` in `main.c`. |
| 3 | #44 | #9 FR-009b export/unset/env | `anshul/fr-009b` | #43 | Registers 3 entries in the `builtins.c` table. The SEC-005 pipeline case is pending. |
| 4 | #45 | #11 NFR-007 error format | `anshul/nfr-007` | #44 | Adds `sh_error()`. The `| ls` case is pending. Lists teammates' messages to update. |
| 5 | #46 | #14 SEC-004 refuse setuid | `anshul/sec-004` | #45 | SEC-T04 runs as root in the CI Docker build. |
| 6 | #47 | #12 SEC-002 no injection | `anshul/sec-002` | #46 | Adds `make check-injection` and a CI step. |
| 7 | #48 | #17 FR-003 `$VAR`/`$?` expansion | `anshul/fr-003` | #47 | Was based on #42; rebased onto the stack. |
| 8 | #49 | #29 SEC-001 ASan fuzz | `anshul/sec-001` | #48 | Adds `make fuzz` and a CI step. Fixes a marker-byte injection in the argv adapter. |

This file's own PR (`anshul/merge-order`) doesn't depend on any of these and can be merged at any time.

## Checked locally (30 Oct)

A trial merge of all 8 PRs, in this order onto `origin/main` (`84e885b`), had **no conflicts**. `make` and `make test` passed after every step, ending at **22/22 suites**, and `make fuzz` reported 0 sanitizer findings. CI (Ubuntu 22.04 and Debian 12) passed on all 8 PRs at the time of writing.

## Pending tests (need FR-005)

These cases print `PENDING: ... needs FR-005 (pipelines) restored on main` instead of failing. They run automatically as soon as `echo pipes | tr a-z A-Z` works again:

- `tests/env_builtins.sh` (#44): `export Z=1 | cat`, then `env` shows no `Z` (SEC-005).
- `tests/negative.sh` (#45): `| ls` gives status 2 with a `simple_shell:` error.

Until FR-005 is back, `export Z=1 | cat` really does change the shell's environment, because the `|` reaches `export` as an argument.

## If a PR needs changes

I'll fix the PR on its own branch and rebase the PRs above it, so the order stays the same. Please don't merge a later PR before an earlier one: it would bring in the earlier PR's commits as well.
