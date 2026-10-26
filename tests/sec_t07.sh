#!/bin/sh
# SEC-T07: child processes must only inherit fds 0, 1, 2.
set -e
BIN=${1:-build/simple_shell}
H=$(mktemp -d)
echo "echo old" > "$H/.simple_shell_history"
chmod 600 "$H/.simple_shell_history"
cat > "$H/probe.sh" <<'PROBE'
#!/bin/sh
n=0
for f in /dev/fd/*; do n=$((n+1)); done
echo "$n"
PROBE
chmod +x "$H/probe.sh"
BASE=$(echo "$H/probe.sh" | HOME=/nonexistent "$BIN")
GOT=$(echo "$H/probe.sh" | HOME="$H" "$BIN")
rm -rf "$H"
if [ "$BASE" != "$GOT" ]; then
  echo "FAIL: fd count differs ($BASE vs $GOT)"; exit 1
fi
echo "sec_t07 OK"
