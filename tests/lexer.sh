#!/bin/sh
# FR-002 integration: an unterminated quote is a syntax error with $? = 2,
# the exact message on stderr, nothing on stdout and nothing executed.
set -eu

shell_bin=${1:-build/simple_shell}
case "$shell_bin" in
    /*) ;;
    *) shell_bin="$(pwd)/${shell_bin#./}" ;;
esac

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
export HOME="$tmpdir"

fail() {
    echo "FAIL: $1"
    exit 1
}

# Unterminated quote is the last line, so the shell exits with its status.
set +e
printf 'touch "%s/ran" "abc\n' "$tmpdir" |
    "$shell_bin" > "$tmpdir/out" 2> "$tmpdir/err"
status=$?
set -e

[ "$status" -eq 2 ] || fail "unterminated quote: status $status, want 2"
[ ! -e "$tmpdir/ran" ] || fail "command ran despite syntax error"
[ ! -s "$tmpdir/out" ] || fail "output on stdout"
printf 'simple_shell: syntax error: unterminated quote\n' > "$tmpdir/want"
cmp -s "$tmpdir/want" "$tmpdir/err" || fail "stderr: $(cat "$tmpdir/err")"

# The shell keeps running after the error, and quoting reaches argv intact.
printf "echo 'oops\necho \"a  b\" 'c \$d' e\\\\ f x>\"%s/o\"\n" "$tmpdir" |
    "$shell_bin" > "$tmpdir/out" 2> "$tmpdir/err" || true
printf 'a  b c $d e f x\n' > "$tmpdir/want"
cmp -s "$tmpdir/want" "$tmpdir/o" || fail "quoted words: $(cat "$tmpdir/o")"

echo "lexer OK"
