#!/bin/sh
# FR-003 integration: $NAME and $? in the running shell, no word splitting,
# and the $V half of SEC-T02 (values are never re-parsed).
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

# $NAME, $UNDEF, quoting, $? after success, failure and not-found.
cat > "$tmpdir/in" <<'EOF'
echo [$HOME] [$UNDEF_X] '[$HOME]' "[$HOME]"
printf '<%s>\n' $SPACED
false
echo $?
nosuchcmd_for_expander
echo $?
true
echo "status $?"
EOF
unset UNDEF_X 2>/dev/null || true
SPACED='a  b' "$shell_bin" < "$tmpdir/in" > "$tmpdir/out" 2> /dev/null
cat > "$tmpdir/want" <<EOF
[$tmpdir] [] [\$HOME] [$tmpdir]
<a  b>
1
127
status 0
EOF
cmp -s "$tmpdir/want" "$tmpdir/out" || fail "expansion output: $(cat "$tmpdir/out")"

# SEC-T02 (second half): V='$(rm /tmp/canary)'; echo $V prints it literally.
printf 'echo $V\nprintf "[%%s]\\n" $W\n' |
    V="\$(rm $canary)" W='x; rm /tmp/canary > /tmp/made | cat' \
    "$shell_bin" > "$tmpdir/out" 2> "$tmpdir/err"
printf '%s\n' "\$(rm $canary)" '[x; rm /tmp/canary > /tmp/made | cat]' > "$tmpdir/want"
cmp -s "$tmpdir/want" "$tmpdir/out" || fail "SEC-T02 \$V: $(cat "$tmpdir/out")"
[ -e "$canary" ] || fail "canary removed via \$V"
[ ! -e /tmp/made ] || fail "expanded > was treated as a redirect"

# IT-BI-02 (Test Plan form, minus the env | grep step that needs pipelines):
# export X=5; unset X; echo [$X]; cd /tmp; pwd; exit 3
set +e
printf 'export X=5\necho [$X]\nunset X\necho [$X]\ncd /tmp\npwd\nexit 3\necho not reached\n' |
    "$shell_bin" > "$tmpdir/out" 2> "$tmpdir/err"
status=$?
set -e
printf '[5]\n[]\n%s\n' "$(cd /tmp && pwd -P)" > "$tmpdir/want"
cmp -s "$tmpdir/want" "$tmpdir/out" || fail "IT-BI-02 output: $(cat "$tmpdir/out")"
[ "$status" -eq 3 ] || fail "IT-BI-02 exit status $status, want 3"

# SEC-T02 exactly as written: export V='$(rm /tmp/canary)'; echo $V
printf "export V='\$(rm %s)'\necho \$V\n" "$canary" | "$shell_bin" > "$tmpdir/out" 2>&1
[ "$(cat "$tmpdir/out")" = "\$(rm $canary)" ] || fail "export V; echo \$V: $(cat "$tmpdir/out")"
[ -e "$canary" ] || fail "canary removed via export V"

echo "expander OK"
