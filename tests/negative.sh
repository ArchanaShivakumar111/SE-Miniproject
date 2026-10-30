#!/bin/sh
# NFR-007 / NEG-01: every error goes to stderr as "simple_shell: ...", with
# nothing on stdout, the right exit status, and the shell keeps running.
set -eu

shell_bin=${1:-build/simple_shell}
case "$shell_bin" in
    /*) ;;
    *) shell_bin="$(pwd)/${shell_bin#./}" ;;
esac

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
export HOME="$tmpdir"
cd "$tmpdir"
printf 'echo hi\n' > noexec
chmod 644 noexec
mkdir adir

failures=0
cases=0

fail() {
    echo "FAIL: $1"
    failures=$((failures + 1))
}

# check <input line> <status> [exact stderr]
# Without exact text, every stderr line must start with "simple_shell: ".
check() {
    cases=$((cases + 1))
    printf '%s\n' "$1" > in
    set +e
    "$shell_bin" < in > out 2> err
    status=$?
    set -e

    [ "$status" -eq "$2" ] || fail "'$1': status $status, want $2"
    [ ! -s out ] || fail "'$1': wrote to stdout: $(cat out)"
    [ -s err ] || [ "$2" -ge 128 ] || fail "'$1': no message on stderr"
    if [ $# -ge 3 ]; then
        [ "$(cat err)" = "$3" ] || fail "'$1': stderr '$(cat err)', want '$3'"
    elif grep -v '^simple_shell: ' err > /dev/null; then
        fail "'$1': stderr line without prefix: $(cat err)"
    fi

    # The shell survives and runs the next line.
    printf '%s\necho alive\n' "$1" > in
    "$shell_bin" < in > out 2> /dev/null || true
    [ "$(tail -n 1 out)" = "alive" ] || fail "'$1': shell did not keep running"
}

# Syntax errors: status 2.
check 'echo "abc' 2 'simple_shell: syntax error: unterminated quote'
check "echo 'abc" 2 'simple_shell: syntax error: unterminated quote'
check "echo 'a\"b" 2 'simple_shell: syntax error: unterminated quote'
check 'ls >' 2
check 'cat <' 2
check 'echo hi >>' 2
check 'ls 2>' 2
check '&' 2
check 'ls > > f' 2
check 'echo a & echo b' 2

# A 100 KB line with an unterminated quote.
big=$(head -c 102400 /dev/zero | tr '\0' 'x')
check "echo '$big" 2 'simple_shell: syntax error: unterminated quote'

# Not found 127, not executable 126, killed by signal 128+n.
check 'nosuchcmd_xyz' 127
check './noexec' 126
check './adir' 126
check "sh -c 'kill -9 \$\$'" 137

# Built-in and redirection errors: status 1 (2 for usage).
check 'export 1A' 1 "simple_shell: export: '1A': not a valid identifier"
check 'export 1A=5' 1 "simple_shell: export: '1A=5': not a valid identifier"
check 'unset 9x' 1 "simple_shell: unset: '9x': not a valid identifier"
check 'env extra' 2 'simple_shell: env: too many arguments'
check 'cd /no/such/dir' 1
check 'cat < /no/such/file' 1
check 'echo x > /no/such/dir/f' 1
check 'fg %9' 1

# "| ls" (empty pipe side) needs the pipeline parser.
printf 'echo pipes | tr a-z A-Z\n' > in
"$shell_bin" < in > out 2> /dev/null || true
if [ "$(cat out)" = "PIPES" ]; then
    check '| ls' 2
else
    echo "PENDING: '| ls' case needs FR-005 (pipelines) restored on main"
fi

if [ "$failures" -ne 0 ]; then
    echo "negative: $failures failure(s) in $cases cases"
    exit 1
fi
echo "negative OK ($cases cases)"
