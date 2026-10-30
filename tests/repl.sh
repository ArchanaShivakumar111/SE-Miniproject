#!/bin/sh
# FR-001 / BR-5: prompt only when interactive, long lines, blank lines and
# Ctrl+D (EOF) exit status.
set -eu

shell_bin=${1:-build/simple_shell}
case "$shell_bin" in
    /*) ;;
    *) shell_bin="$(pwd)/${shell_bin#./}" ;;
esac

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
export HOME="$tmpdir/home"
mkdir -p "$HOME/w"

fail() {
    echo "FAIL: $1"
    exit 1
}

# ST-REPL-01 (non-interactive part): a 64 KiB echo line is echoed in full
# and no prompt is printed when stdin is not a terminal.
big=$(head -c 65536 /dev/zero | tr '\0' 'x')
printf 'echo %s\n' "$big" | "$shell_bin" > "$tmpdir/out" 2> "$tmpdir/err"
printf '%s\n' "$big" > "$tmpdir/want"
cmp -s "$tmpdir/want" "$tmpdir/out" || fail "64 KiB line not echoed exactly (or prompt printed)"
[ ! -s "$tmpdir/err" ] || fail "64 KiB line wrote to stderr"

# Ctrl+D (EOF) on an empty line exits with the last status.
set +e
printf 'sh -c "exit 3"\n' | "$shell_bin" > /dev/null 2>&1
status=$?
set -e
[ "$status" -eq 3 ] || fail "EOF exit status $status, want 3"

# BR-5: blank and whitespace-only lines leave $? unchanged...
set +e
printf 'false\n\n   \n\t\n' | "$shell_bin" > /dev/null 2>&1
status=$?
set -e
[ "$status" -eq 1 ] || fail "blank lines changed \$? to $status, want 1"

# ...and are not added to history.
rm -f "$HOME/.simple_shell_history"
printf 'echo a\n\n   \n\t\necho b\n' | "$shell_bin" > /dev/null 2>&1
printf 'echo a\necho b\n' > "$tmpdir/want"
cmp -s "$tmpdir/want" "$HOME/.simple_shell_history" ||
    fail "blank lines reached history: $(cat "$HOME/.simple_shell_history")"

# ST-REPL-01 (interactive part): real prompt on a pseudo-terminal, with
# home shown as ~ and updated after cd.
if command -v script > /dev/null 2>&1 &&
   script -qec true /dev/null > /dev/null 2>&1; then
    user=$(id -un)
    host=$(uname -n)
    (cd "$HOME/w" &&
     printf 'cd ..\ncd /\nexit 4\n' |
         timeout 10 script -qec "$shell_bin" /dev/null) > "$tmpdir/pty" 2>&1 || true
    for want in "$user@$host:~/w\$ " "$user@$host:~\$ " "$user@$host:/\$ "; do
        grep -qF "$want" "$tmpdir/pty" || fail "pty prompt '$want' not found in: $(cat "$tmpdir/pty")"
    done
else
    echo "SKIP: pty prompt test ('script' not available)"
fi

echo "repl OK"
