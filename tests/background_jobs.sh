#!/bin/sh
set -eu

BIN=${1:-build/simple_shell}
case "$BIN" in
    /*) ;;
    *) BIN="$(pwd)/${BIN#./}" ;;
esac

TMP=$(mktemp -d)
mkdir -p "$TMP/home"

cleanup() {
    if [ -f "$TMP/out" ]; then
        for pid in $(awk '/^\[[0-9]+\] [0-9]+$/ {print $2}' "$TMP/out"); do
            kill "$pid" 2>/dev/null || true
        done
    fi
    rm -rf "$TMP"
}
trap cleanup EXIT HUP INT TERM

cat > "$TMP/commands" <<'COMMANDS'
/bin/sleep 3 &
/bin/sleep 3 &
echo AFTER
COMMANDS

start=$(date +%s%N)
HOME="$TMP/home" "$BIN" < "$TMP/commands" \
    > "$TMP/out" 2> "$TMP/err"
end=$(date +%s%N)

elapsed_ms=$(( (end - start) / 1000000 ))

if [ "$elapsed_ms" -ge 2500 ]; then
    echo "FAIL: background execution took ${elapsed_ms}ms"
    exit 1
fi

if ! grep -Eq '^\[1\] [0-9]+$' "$TMP/out"; then
    echo "FAIL: first job ID and PID not printed"
    exit 1
fi

if ! grep -Eq '^\[2\] [0-9]+$' "$TMP/out"; then
    echo "FAIL: second job ID and PID not printed"
    exit 1
fi

if ! grep -qx 'AFTER' "$TMP/out"; then
    echo "FAIL: shell did not continue to the next command"
    exit 1
fi

echo "background_jobs OK"
