#!/bin/sh
# Test suite. Add new checks at the bottom (one block per feature).
set -e
BIN=./simple_shell

fail() { echo "FAIL: $1"; exit 1; }

[ -x "$BIN" ] || fail "$BIN not built"

# --- skeleton (#1) ---
echo "" | $BIN
echo "PASS: shell starts and exits cleanly on EOF"

# --- FR-004: external commands via PATH (#5) ---
out=$(echo "echo hello world" | $BIN)
[ "$out" = "hello world" ] || fail "FR-004 runs external command (got '$out')"
echo "PASS: FR-004 runs an external command found via PATH"

err=$(echo "nosuchcmd_xyz" | $BIN 2>&1 >/dev/null)
[ "$err" = "simple_shell: nosuchcmd_xyz: command not found" ] \
    || fail "FR-004 unknown command message (got '$err')"
echo "PASS: FR-004 unknown command reports 'command not found' on stderr"

out=$(printf 'nosuchcmd_xyz\necho still alive\n' | $BIN 2>/dev/null)
[ "$out" = "still alive" ] || fail "FR-004 shell survives a failed command"
echo "PASS: FR-004 shell keeps running after a failed command"

out=$(printf '   echo   spaced   out  \n' | $BIN)
[ "$out" = "spaced out" ] || fail "FR-004 extra whitespace (got '$out')"
echo "PASS: FR-004 handles extra whitespace"

# --- FR-005: multi-stage pipelines (#6) ---
out=$(echo "echo hello | tr a-z A-Z" | $BIN)
[ "$out" = "HELLO" ] || fail "FR-005 two-stage pipeline (got '$out')"
echo "PASS: FR-005 two-stage pipeline"

out=$(echo "echo hello | tr a-z A-Z | rev" | $BIN)
[ "$out" = "OLLEH" ] || fail "FR-005 three-stage pipeline (got '$out')"
echo "PASS: FR-005 three-stage pipeline"

line="echo hi"; i=1
while [ $i -lt 16 ]; do line="$line | cat"; i=$((i + 1)); done
out=$(echo "$line" | timeout 10 $BIN)
[ "$out" = "hi" ] || fail "FR-005 16-stage pipeline (got '$out')"
echo "PASS: FR-005 16-stage pipeline (maximum)"

line="echo hi"; i=1
while [ $i -lt 17 ]; do line="$line | cat"; i=$((i + 1)); done
err=$(echo "$line" | $BIN 2>&1 >/dev/null)
[ "$err" = "simple_shell: too many pipeline stages (max 16)" ] \
    || fail "FR-005 17 stages rejected (got '$err')"
echo "PASS: FR-005 17-stage pipeline is rejected"

out=$(echo "yes | head -n 1" | timeout 5 $BIN)
[ "$out" = "y" ] || fail "FR-005 early-exit reader, no hang (got '$out')"
echo "PASS: FR-005 'yes | head -n 1' finishes (SIGPIPE handled)"

err=$(printf 'ls | | cat\n' | $BIN 2>&1 >/dev/null)
[ "$err" = "simple_shell: syntax error near unexpected token '|'" ] \
    || fail "FR-005 empty stage (got '$err')"
echo "PASS: FR-005 empty pipeline stage is a syntax error"

out=$(printf 'echo a | nosuchcmd_xyz\necho still alive\n' | $BIN 2>/dev/null)
[ "$out" = "still alive" ] || fail "FR-005 shell survives a failed stage"
echo "PASS: FR-005 shell survives a failing pipeline stage"
