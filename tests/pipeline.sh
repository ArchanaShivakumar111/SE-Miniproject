#!/bin/sh
# FR-005 / SEC-005 / NFR-003: multi-stage pipelines (2 to 16 stages).
set -eu

shell_bin=${1:-build/simple_shell}
case "$shell_bin" in
    /*) ;;
    *) shell_bin="$(pwd)/${shell_bin#./}" ;;
esac

tmpdir=$(mktemp -d)
cleanup() {
    [ -z "${shell_pid:-}" ] || kill "$shell_pid" 2>/dev/null || true
    [ -z "${writer_pid:-}" ] || kill "$writer_pid" 2>/dev/null || true
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM
export HOME="$tmpdir"
cd "$tmpdir"

fail() {
    echo "FAIL: $1"
    exit 1
}

# run <input> : runs the shell on the text, stdout in "out", stderr in "err",
# exit status in "$status".
run() {
    printf '%s\n' "$1" > in
    set +e
    timeout 10 "$shell_bin" < in > out 2> err
    status=$?
    set -e
}

# Two and three stages; a built-in (echo) and external commands.
run 'echo hello | tr a-z A-Z'
[ "$(cat out)" = "HELLO" ] || fail "two-stage pipeline: $(cat out)"

run 'echo hello | cat | tr a-z A-Z | rev'
[ "$(cat out)" = "OLLEH" ] || fail "four-stage pipeline: $(cat out)"

run 'printf "b\na\nc\n" | sort | head -n 2'
[ "$(tr '\n' ' ' < out)" = "a b " ] || fail "external stages: $(cat out)"

# The maximum is 16 stages; a 17th is refused with status 2.
line='echo hi'; i=1
while [ "$i" -lt 16 ]; do line="$line | cat"; i=$((i + 1)); done
run "$line"
[ "$(cat out)" = "hi" ] || fail "16-stage pipeline: $(cat out)"

line='echo hi'; i=1
while [ "$i" -lt 17 ]; do line="$line | cat"; i=$((i + 1)); done
run "$line"
[ ! -s out ] || fail "17 stages wrote to stdout: $(cat out)"
[ "$status" -eq 2 ] || fail "17 stages: shell exit status $status, want 2"
[ "$(cat err)" = "simple_shell: too many pipeline stages (max 16)" ] ||
    fail "17 stages message: $(cat err)"
printf '%s\necho $?\n' "$line" > in
"$shell_bin" < in > out 2> /dev/null
[ "$(cat out)" = "2" ] || fail "17 stages: status $(cat out), want 2"

# A '|' inside quotes is an ordinary character.
run "echo \"a|b\" 'x | y'"
[ "$(cat out)" = "a|b x | y" ] || fail "quoted pipe: $(cat out)"

# $? is the exit status of the last stage.
printf 'true | false\necho $?\nfalse | true\necho $?\n' > in
"$shell_bin" < in > out 2> /dev/null
[ "$(tr '\n' ' ' < out)" = "1 0 " ] || fail "last-stage status: $(cat out)"

# A reader that stops early must not hang the writer (SIGPIPE).
run 'yes | head -n 1'
[ "$(cat out)" = "y" ] || fail "yes | head -n 1: $(cat out)"

# Redirections apply per stage; the pipe is set up first.
printf 'abc\n' > data
run 'cat < data | tr a-z A-Z > upper'
[ "$(cat upper)" = "ABC" ] || fail "< and > in a pipeline: $(cat upper)"
run 'echo hi > only_file | cat'
[ ! -s out ] && [ "$(cat only_file)" = "hi" ] ||
    fail "'echo hi > f | cat': stdout '$(cat out)'"

# SEC-005: built-ins in a pipeline run in a child and leave the shell alone.
mkdir sub
printf 'export Z=1 | cat\ncd sub | cat\nunset HOME | cat\npwd\nenv > envout\n' > in
"$shell_bin" < in > out 2> /dev/null
[ "$(grep -c '^Z=' envout || true)" = "0" ] || fail "export in a pipeline changed the shell"
[ "$(grep '^HOME=' envout)" = "HOME=$tmpdir" ] || fail "unset in a pipeline changed the shell"
[ "$(cat out)" = "$tmpdir" ] || fail "cd in a pipeline changed the directory: $(cat out)"
printf 'echo a | exit 3\necho alive\n' > in
"$shell_bin" < in > out 2> /dev/null
[ "$(cat out)" = "alive" ] || fail "exit in a pipeline stopped the shell"

# Syntax errors: status 2, message on stderr, nothing runs, shell survives.
for bad in '| ls' 'ls |' 'ls | | cat' '> f | cat'; do
    printf '%s\necho alive\n' "$bad" > in
    "$shell_bin" < in > out 2> err || true
    [ "$(cat out)" = "alive" ] || fail "'$bad': shell did not keep running"
    grep -q '^simple_shell: syntax error' err || fail "'$bad': message: $(cat err)"
    [ "$(grep -vc '^simple_shell: ' err || true)" = "0" ] || fail "'$bad': unprefixed stderr"
    printf '%s\necho $?\n' "$bad" > in
    "$shell_bin" < in > out 2> /dev/null || true
    [ "$(cat out)" = "2" ] || fail "'$bad': status $(cat out), want 2"
done

# A stage that cannot run: its message, status 127, others finish.
printf 'echo a | nosuchcmd_xyz\necho $?\n' > in
"$shell_bin" < in > out 2> err
[ "$(cat out)" = "127" ] || fail "missing command status: $(cat out)"
[ "$(cat err)" = "simple_shell: nosuchcmd_xyz: No such file or directory" ] ||
    fail "missing command message: $(cat err)"

# Aliases expand in every stage, and an alias may itself hold a pipe.
printf "alias up='tr a-z A-Z'\necho hi | up\nalias two='echo yo | tr a-z A-Z'\ntwo\n" > in
"$shell_bin" < in > out 2> /dev/null
[ "$(tr '\n' ' ' < out)" = "HI YO " ] || fail "aliases in pipelines: $(cat out)"

# A background pipeline returns at once, prints [id] pid, and is a job.
start=$(date +%s)
printf 'sleep 2 | cat &\necho AFTER\njobs\n' > in
"$shell_bin" < in > out 2> /dev/null
elapsed=$(( $(date +%s) - start ))
[ "$elapsed" -lt 2 ] || fail "background pipeline blocked for ${elapsed}s"
grep -Eq '^\[1\] [0-9]+$' out || fail "no [1] pid line for a background pipeline"
grep -qx 'AFTER' out || fail "shell did not continue after a background pipeline"
grep -q '^\[1\] Running  *sleep 2 | cat &$' out || fail "jobs does not list the pipeline: $(cat out)"

# NFR-003: no stage is left as a zombie or still running once it is done.
mkfifo fifo
( printf 'echo a | cat | cat\n'; sleep 3 ) > fifo &
writer_pid=$!
"$shell_bin" < fifo > /dev/null 2>&1 &
shell_pid=$!
sleep 1
children=$(cat "/proc/$shell_pid/task/$shell_pid/children" 2>/dev/null || true)
[ -z "$children" ] || fail "shell still has children after a pipeline: $children"

echo "pipeline OK"
