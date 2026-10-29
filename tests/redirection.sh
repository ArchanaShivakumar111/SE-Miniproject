#!/bin/sh
set -eu

shell_bin=${1:-build/simple_shell}
case "$shell_bin" in
    /*) ;;
    *) shell_bin="$(pwd)/${shell_bin#./}" ;;
esac

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
mkdir -p "$tmpdir/home"
export HOME="$tmpdir/home"

# Verify overwrite, append, input redirection, and restoration.
printf 'old\n' > "$tmpdir/out"

cat > "$tmpdir/commands" <<EOF_COMMANDS
echo alpha > "$tmpdir/out"
echo beta >> "$tmpdir/out"
echo restored
cat < "$tmpdir/out" > "$tmpdir/copy"
echo attached>"$tmpdir/nospace"
ls "$tmpdir/no_such_file" 2> "$tmpdir/err"
sh -c 'printf OUT; printf ERR >&2' &> "$tmpdir/both"
EOF_COMMANDS

"$shell_bin" < "$tmpdir/commands" \
    > "$tmpdir/stdout" 2> "$tmpdir/shell-stderr"

printf 'alpha\nbeta\n' > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/out"
cmp "$tmpdir/expected" "$tmpdir/copy"

printf 'restored\n' > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/stdout"

printf 'attached\n' > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/nospace"

test -s "$tmpdir/err"

printf 'OUTERR' > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/both"

# An output-file open failure must fail the command.
set +e
printf 'echo SHOULD_NOT_APPEAR > "%s/missing/out"\n' "$tmpdir" |
    "$shell_bin" > "$tmpdir/fail-stdout" 2> "$tmpdir/fail-stderr"
status=$?
set -e

if [ "$status" -eq 0 ]; then
    echo "FAIL: invalid output path returned success"
    exit 1
fi

if grep -q 'SHOULD_NOT_APPEAR' "$tmpdir/fail-stdout"; then
    echo "FAIL: command ran despite redirection failure"
    exit 1
fi

test -s "$tmpdir/fail-stderr"

echo "redirection OK"
