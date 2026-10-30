#!/bin/sh
# SEC-T03: redirection must respect file permissions.
set -eu

BIN=${1:-build/simple_shell}
case "$BIN" in
    /*) ;;
    *) BIN="$(pwd)/${BIN#./}" ;;
esac

TMP=$(mktemp -d)
chmod 755 "$TMP"
trap 'chmod -R u+rwx "$TMP"; rm -rf "$TMP"' EXIT HUP INT TERM

# Root-based CI must run the checks as an unprivileged user.
if [ "$(id -u)" -eq 0 ]; then
    if ! command -v runuser >/dev/null 2>&1; then
        echo "FAIL: runuser is required to test permissions as root" >&2
        exit 1
    fi
    run_shell() {
        runuser -u nobody -- env HOME=/nonexistent "$BIN"
    }
else
    run_shell() {
        env HOME=/nonexistent "$BIN"
    }
fi

# A read-only file must not be overwritten.
printf 'KEEP_THIS_CONTENT\n' > "$TMP/protected.txt"
chmod 444 "$TMP/protected.txt"

set +e
printf 'echo SHOULD_NOT_WRITE > %s/protected.txt\n' "$TMP" |
    run_shell > "$TMP/out" 2> "$TMP/err"
status=$?
set -e

if [ "$status" -eq 0 ]; then
    echo "FAIL: overwriting a read-only file succeeded"
    exit 1
fi

if [ "$(cat "$TMP/protected.txt")" != "KEEP_THIS_CONTENT" ]; then
    echo "FAIL: protected file contents changed"
    exit 1
fi

if grep -q 'SHOULD_NOT_WRITE' "$TMP/out"; then
    echo "FAIL: command ran after output redirection failed"
    exit 1
fi

test -s "$TMP/err"

# An unreadable input file must prevent command execution.
printf 'SECRET_INPUT\n' > "$TMP/unreadable.txt"
chmod 000 "$TMP/unreadable.txt"

set +e
printf 'cat < %s/unreadable.txt\n' "$TMP" |
    run_shell > "$TMP/input-out" 2> "$TMP/input-err"
status=$?
set -e

if [ "$status" -eq 0 ]; then
    echo "FAIL: reading a permission-denied file succeeded"
    exit 1
fi

if grep -q 'SECRET_INPUT' "$TMP/input-out"; then
    echo "FAIL: command ran after input redirection failed"
    exit 1
fi

test -s "$TMP/input-err"

echo "sec_t03 OK"
