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
