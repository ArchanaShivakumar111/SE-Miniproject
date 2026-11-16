#!/bin/sh
# SEC-T07: child processes must only inherit fds 0, 1, 2.
# The history file is only opened by an interactive shell (FR-012), so the
# shell runs on a pseudo-terminal with the `script` command.
set -e
BIN=${1:-build/simple_shell}

if ! command -v script > /dev/null 2>&1 ||
   ! script -qec true /dev/null > /dev/null 2>&1; then
  echo "SKIP: SEC-T07 needs a pseudo-terminal ('script' not available)"
  exit 0
fi

case "$BIN" in
  /*) ;;
  *) BIN="$(pwd)/${BIN#./}" ;;
esac

H=$(mktemp -d)
echo "echo old" > "$H/.simple_shell_history"
chmod 600 "$H/.simple_shell_history"
cat > "$H/probe.sh" <<'PROBE'
#!/bin/sh
n=0
for f in /dev/fd/*; do n=$((n+1)); done
echo "FDCOUNT $n"
PROBE
chmod +x "$H/probe.sh"

count() {
  printf '%s\nexit\n' "$H/probe.sh" | HOME="$1" timeout 10 script -qec "$BIN" /dev/null |
    tr -d '\r' | sed -n 's/.*FDCOUNT \([0-9][0-9]*\).*/\1/p' | head -1
}

BASE=$(count /nonexistent)
GOT=$(count "$H")
# The history file must really have been loaded and saved in the second run.
saved=$(wc -l < "$H/.simple_shell_history")
rm -rf "$H"
[ -n "$BASE" ] && [ -n "$GOT" ] || { echo "FAIL: probe printed no fd count"; exit 1; }
[ "$saved" -ge 3 ] || { echo "FAIL: history was not used ($saved lines)"; exit 1; }
if [ "$BASE" != "$GOT" ]; then
  echo "FAIL: fd count differs ($BASE vs $GOT)"; exit 1
fi
echo "sec_t07 OK"
