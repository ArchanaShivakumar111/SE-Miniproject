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

echo "expander OK"
