#!/bin/sh
# FR-012: script (non-interactive) mode: simple_shell script.sh
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

fail() {
    echo "FAIL: $1"
    exit 1
}

# Commands run in order; pipelines, variables and comments work; no prompt.
cat > basic.sh <<'SCRIPT'
#!/usr/local/bin/simple_shell
# a comment, then a blank line

echo first
export W=hello
echo $W | tr a-z A-Z
   # an indented comment
echo last
SCRIPT
"$shell_bin" basic.sh < /dev/null > out 2> err
[ "$(tr '\n' ' ' < out)" = "first HELLO last " ] || fail "script output: $(cat out)"
[ ! -s err ] || fail "script wrote to stderr: $(cat err)"

# The exit status is the status of the last command, or of exit N.
printf 'echo a\nfalse\n' > last.sh
set +e
"$shell_bin" last.sh < /dev/null > /dev/null 2>&1
status=$?
set -e
[ "$status" -eq 1 ] || fail "last command false: status $status, want 1"

printf 'echo a\nexit 7\necho not reached\n' > exit7.sh
set +e
"$shell_bin" exit7.sh < /dev/null > out 2>&1
status=$?
set -e
[ "$status" -eq 7 ] || fail "exit 7: status $status, want 7"
[ "$(cat out)" = "a" ] || fail "commands ran after exit: $(cat out)"

# A comment does not change $?.
printf 'false\n# nothing\necho $?\n' > comment.sh
"$shell_bin" comment.sh < /dev/null > out 2>&1
[ "$(cat out)" = "1" ] || fail "a comment changed \$?: $(cat out)"

# The script is not stdin: cat reads the real stdin, not the script.
printf 'echo one\ncat\necho two\n' > cat.sh
printf 'DATA\n' | "$shell_bin" cat.sh > out 2>&1
[ "$(tr '\n' ' ' < out)" = "one DATA two " ] || fail "cat inside a script: $(cat out)"

# SEC-007: the script's own descriptor is not passed on to commands. The
# same listing is taken with piped input, so descriptors the caller leaves
# open (a terminal often does) do not matter.
cat > fds.sh <<'SCRIPT'
/bin/ls /proc/self/fd
SCRIPT
"$shell_bin" fds.sh < /dev/null > got.fds 2>&1
"$shell_bin" < fds.sh > base.fds 2>&1
[ "$(grep -c . got.fds)" = "$(grep -c . base.fds)" ] ||
    fail "script descriptor leaked: $(tr '\n' ' ' < got.fds) (piped: $(tr '\n' ' ' < base.fds))"

# A script does not touch the history file; piped stdin still can.
rm -f "$HOME/.simple_shell_history"
"$shell_bin" basic.sh < /dev/null > /dev/null 2>&1
[ ! -e "$HOME/.simple_shell_history" ] || fail "script mode wrote a history file"

# Errors: missing file 127, directory 126, unreadable 126, extra args 2.
set +e
"$shell_bin" nosuch.sh > out 2> err
status=$?
set -e
[ "$status" -eq 127 ] || fail "missing script: status $status, want 127"
[ "$(cat err)" = "simple_shell: nosuch.sh: No such file or directory" ] ||
    fail "missing script message: $(cat err)"
[ ! -s out ] || fail "missing script wrote to stdout"

mkdir adir
set +e
"$shell_bin" adir > out 2> err
status=$?
set -e
[ "$status" -eq 126 ] || fail "directory: status $status, want 126"
[ "$(cat err)" = "simple_shell: adir: Is a directory" ] || fail "directory message: $(cat err)"

if [ "$(id -u)" -ne 0 ]; then
    printf 'echo hi\n' > secret.sh
    chmod 000 secret.sh
    set +e
    "$shell_bin" secret.sh > out 2> err
    status=$?
    set -e
    [ "$status" -eq 126 ] || fail "unreadable script: status $status, want 126"
    grep -q '^simple_shell: secret.sh: Permission denied$' err ||
        fail "unreadable script message: $(cat err)"
fi

set +e
"$shell_bin" basic.sh extra > out 2> err
status=$?
set -e
[ "$status" -eq 2 ] || fail "extra argument: status $status, want 2"
[ "$(cat err)" = "simple_shell: usage: simple_shell [script]" ] ||
    fail "usage message: $(cat err)"

# A script with an error in it keeps going to the next line.
printf 'nosuchcmd_xyz\necho after\n' > keep.sh
"$shell_bin" keep.sh < /dev/null > out 2> err || true
[ "$(cat out)" = "after" ] || fail "script stopped at a failing command: $(cat out)"

# Piped stdin works as before.
printf 'echo piped\n' | "$shell_bin" > out 2>&1
[ "$(cat out)" = "piped" ] || fail "piped stdin: $(cat out)"

echo "script OK"
