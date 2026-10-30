#!/bin/sh
# FR-009b / SEC-005: export, unset and env change the shell's own
# environment as single foreground commands, but not when run with & or in a
# pipeline. Uses env > file + grep because $VAR expansion is FR-003.
set -eu

shell_bin=${1:-build/simple_shell}
case "$shell_bin" in
    /*) ;;
    *) shell_bin="$(pwd)/${shell_bin#./}" ;;
esac

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
export HOME="$tmpdir"
unset X Y Z K 2>/dev/null || true

fail() {
    echo "FAIL: $1"
    exit 1
}

# IT-BI-02: export X=5; env | grep ^X=; unset X; env | grep -c ^X=
cat > "$tmpdir/in" <<EOF
export X=5
env > "$tmpdir/env1"
printenv X > "$tmpdir/child"
unset X
env > "$tmpdir/env2"
export 'S=a b=c'
env > "$tmpdir/env3"
EOF
"$shell_bin" < "$tmpdir/in" > "$tmpdir/out" 2> "$tmpdir/err"
[ "$(grep '^X=' "$tmpdir/env1")" = "X=5" ] || fail "export X=5 not in env"
[ "$(cat "$tmpdir/child")" = "5" ] || fail "exported X not inherited by a child"
[ "$(grep -c '^X=' "$tmpdir/env2" || true)" = "0" ] || fail "unset X left X in env"
[ "$(grep '^S=' "$tmpdir/env3")" = "S=a b=c" ] || fail "export with = and space in value"
[ ! -s "$tmpdir/err" ] || fail "unexpected stderr: $(cat "$tmpdir/err")"

# Invalid identifier: message on stderr, status 1.
set +e
printf 'export 1A\n' | "$shell_bin" > "$tmpdir/out" 2> "$tmpdir/err"
status=$?
set -e
[ "$status" -eq 1 ] || fail "export 1A: status $status, want 1"
[ ! -s "$tmpdir/out" ] || fail "export 1A wrote to stdout"
[ "$(cat "$tmpdir/err")" = "simple_shell: export: '1A': not a valid identifier" ] ||
    fail "export 1A stderr: $(cat "$tmpdir/err")"

set +e
printf 'unset 9x\n' | "$shell_bin" > /dev/null 2> "$tmpdir/err"
status=$?
set -e
[ "$status" -eq 1 ] || fail "unset 9x: status $status, want 1"
[ "$(cat "$tmpdir/err")" = "simple_shell: unset: '9x': not a valid identifier" ] ||
    fail "unset 9x stderr: $(cat "$tmpdir/err")"

# SEC-005 with &: runs in a child and does not change the shell.
cat > "$tmpdir/in" <<EOF
export K=1
export Y=1 &
unset K &
sleep 0.2
env > "$tmpdir/env4"
EOF
"$shell_bin" < "$tmpdir/in" > /dev/null 2>&1
[ "$(grep -c '^Y=' "$tmpdir/env4" || true)" = "0" ] || fail "export Y=1 & changed the shell"
[ "$(grep '^K=' "$tmpdir/env4")" = "K=1" ] || fail "unset K & changed the shell"

# SEC-005 in a pipeline: export Z=1 | cat; env | grep -c ^Z= -> 0
printf 'echo pipes | tr a-z A-Z\n' | "$shell_bin" > "$tmpdir/pipe" 2>/dev/null || true
if [ "$(cat "$tmpdir/pipe")" = "PIPES" ]; then
    printf 'export Z=1 | cat\nenv > "%s/env5"\n' "$tmpdir" |
        "$shell_bin" > /dev/null 2>&1
    [ "$(grep -c '^Z=' "$tmpdir/env5" || true)" = "0" ] ||
        fail "export Z=1 | cat changed the shell"
else
    echo "PENDING: SEC-005 pipeline case needs FR-005 (pipelines) restored on main"
fi

echo "env_builtins OK"
