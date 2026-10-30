#!/bin/sh
set -eu

BIN=${1:-build/simple_shell}
case "$BIN" in
    /*) ;;
    *) BIN="$(pwd)/${BIN#./}" ;;
esac

TMP=$(mktemp -d)
mkdir -p "$TMP/home"
mkfifo "$TMP/input"

SHELL_PID=
WRITER_PID=
CHILD_PID=

cleanup() {
    if [ -n "$CHILD_PID" ]; then
        kill "$CHILD_PID" 2>/dev/null || true
    fi

    if [ -n "$SHELL_PID" ]; then
        kill "$SHELL_PID" 2>/dev/null || true
        wait "$SHELL_PID" 2>/dev/null || true
    fi

    if [ -n "$WRITER_PID" ]; then
        kill "$WRITER_PID" 2>/dev/null || true
        wait "$WRITER_PID" 2>/dev/null || true
    fi

    rm -rf "$TMP"
}

trap cleanup EXIT HUP INT TERM

# Keep the input pipe open while the background child finishes.
(
    printf '/bin/sleep 0.5 &\n'
    sleep 3
) > "$TMP/input" &
WRITER_PID=$!

HOME="$TMP/home" "$BIN" < "$TMP/input" \
    > "$TMP/out" 2> "$TMP/err" &
SHELL_PID=$!

# Find the shell's child through /proc; this does not require ps.
attempt=0
while [ "$attempt" -lt 100 ]; do
    children=$(cat "/proc/$SHELL_PID/task/$SHELL_PID/children" \
        2>/dev/null || true)
    CHILD_PID=${children%% *}

    if [ -n "$CHILD_PID" ]; then
        break
    fi

    attempt=$((attempt + 1))
    sleep 0.02
done

if [ -z "$CHILD_PID" ]; then
    echo "FAIL: background child was not started"
    cat "$TMP/err"
    exit 1
fi

# Allow the child to finish while the shell is waiting for input.
sleep 1

# Linux /proc/<pid>/stat reports the process state as its third field.
# Remove the parenthesized command name first, since it may contain spaces.
state=$(
    if [ -r "/proc/$CHILD_PID/stat" ]; then
        sed 's/^.*) //' "/proc/$CHILD_PID/stat" |
            awk '{print $1}'
    fi
)

case "$state" in
    Z*)
        echo "FAIL: completed child $CHILD_PID remained a zombie"
        exit 1
        ;;
    "")
        echo "zombie_processes OK"
        ;;
    *)
        echo "FAIL: child $CHILD_PID was still present with state $state"
        exit 1
        ;;
esac
