#!/bin/sh
# SEC-T04 / SEC-004: a setuid or setgid copy of the shell refuses to run.
# Needs root to give the binary to another owner; skipped otherwise.
set -eu

shell_bin=${1:-build/simple_shell}
case "$shell_bin" in
    /*) ;;
    *) shell_bin="$(pwd)/${shell_bin#./}" ;;
esac

fail() {
    echo "FAIL: $1"
    exit 1
}

# Normal start: no refusal.
printf 'echo ok\n' | "$shell_bin" > /dev/null 2>&1 || fail "shell refused a normal start"

if [ "$(id -u)" -ne 0 ]; then
    echo "SKIP: SEC-T04 needs root to create a setuid binary (unit test covers the check)"
    echo "sec_t04 OK"
    exit 0
fi

other=$(id -u nobody 2>/dev/null || true)
other_group=$(id -g nobody 2>/dev/null || true)
if [ -z "$other" ] || [ -z "$other_group" ]; then
    echo "SKIP: SEC-T04 needs a 'nobody' user"
    echo "sec_t04 OK"
    exit 0
fi

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
chmod 755 "$tmpdir"
want='simple_shell: refusing to run with elevated privileges'

# run_case <mode> <description>
run_case() {
    cp "$shell_bin" "$tmpdir/sh_$1"
    chown "$other:$other_group" "$tmpdir/sh_$1"
    chmod "$1" "$tmpdir/sh_$1"

    # The setuid/setgid bit only takes effect where the mount allows it.
    if [ ! -u "$tmpdir/sh_$1" ] && [ ! -g "$tmpdir/sh_$1" ]; then
        fail "$2: could not set mode $1"
    fi

    set +e
    printf 'echo SHOULD_NOT_RUN\n' |
        "$tmpdir/sh_$1" > "$tmpdir/out" 2> "$tmpdir/err"
    status=$?
    set -e

    [ "$status" -eq 1 ] || fail "$2: status $status, want 1"
    [ ! -s "$tmpdir/out" ] || fail "$2: ran a command: $(cat "$tmpdir/out")"
    [ "$(cat "$tmpdir/err")" = "$want" ] || fail "$2: stderr '$(cat "$tmpdir/err")'"
}

run_case 4755 "setuid"
run_case 2755 "setgid"

echo "sec_t04 OK"
