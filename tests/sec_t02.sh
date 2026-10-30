#!/bin/sh
# SEC-T02 / SEC-002: shell metacharacters inside quotes reach the program as
# literal bytes (argv exactly as tokenized), and the sources contain no
# system(), popen() or exec of /bin/sh. The $V half of SEC-T02 needs $VAR
# expansion and is in tests/expander.sh (FR-003).
set -eu

shell_bin=${1:-build/simple_shell}
case "$shell_bin" in
    /*) ;;
    *) shell_bin="$(pwd)/${shell_bin#./}" ;;
esac

tmpdir=$(mktemp -d)
canary=/tmp/canary
created=0
if [ ! -e "$canary" ]; then
    : > "$canary"
    created=1
fi
cleanup() {
    rm -rf "$tmpdir"
    if [ "$created" -eq 1 ]; then rm -f "$canary"; fi
}
trap cleanup EXIT HUP INT TERM
export HOME="$tmpdir"

fail() {
    echo "FAIL: $1"
    exit 1
}

# echo "x; rm /tmp/canary" prints literally and the canary survives.
printf 'echo "x; rm %s"\n' "$canary" | "$shell_bin" > "$tmpdir/out" 2> "$tmpdir/err"
[ "$(cat "$tmpdir/out")" = "x; rm $canary" ] || fail "echo output: $(cat "$tmpdir/out")"
[ -e "$canary" ] || fail "canary removed by echo \"x; rm $canary\""
[ ! -s "$tmpdir/err" ] || fail "stderr: $(cat "$tmpdir/err")"

# argv reaches execvp exactly as tokenized: one argument per quoted word,
# ; ` $( ) | & > inside quotes stay literal (external printf shows argv).
cat > "$tmpdir/in" <<EOF
printf '[%s]\n' "a; rm $canary" 'b | rm $canary' '\$(rm $canary)' "\`rm $canary\`" 'c && rm $canary' 'd > $tmpdir/made' "e & f"
EOF
"$shell_bin" < "$tmpdir/in" > "$tmpdir/out" 2> "$tmpdir/err"
cat > "$tmpdir/want" <<EOF
[a; rm $canary]
[b | rm $canary]
[\$(rm $canary)]
[\`rm $canary\`]
[c && rm $canary]
[d > $tmpdir/made]
[e & f]
EOF
cmp -s "$tmpdir/want" "$tmpdir/out" || fail "argv not literal: $(cat "$tmpdir/out")"
[ -e "$canary" ] || fail "canary removed by quoted metacharacters"
[ ! -e "$tmpdir/made" ] || fail "quoted > created a file"

# Source check passes on the real sources...
make -s check-injection > "$tmpdir/check" 2>&1 || fail "check-injection: $(cat "$tmpdir/check")"

# ...and really fails when a banned call is present.
for bad in 'system("ls");' 'FILE *p = popen("ls", "r");' 'execl("/bin/sh", "sh", NULL);'; do
    mkdir -p "$tmpdir/fake"
    printf 'void f(void) { %s }\n' "$bad" > "$tmpdir/fake/bad.c"
    if make -s check-injection INJECTION_DIRS="$tmpdir/fake" > /dev/null 2>&1; then
        fail "check-injection missed: $bad"
    fi
done
printf 'int filesystem(void);\n' > "$tmpdir/fake/bad.c"
make -s check-injection INJECTION_DIRS="$tmpdir/fake" > /dev/null 2>&1 ||
    fail "check-injection flagged filesystem()"

echo "sec_t02 OK"
