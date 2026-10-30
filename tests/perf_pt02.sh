#!/bin/sh
# PT-02: first prompt must appear within 100 ms with a full (1,000-entry) history.
BIN=${1:-build/simple_shell}
LIMIT_MS=${2:-100}
H=$(mktemp -d)
i=0
while [ $i -lt 1000 ]; do echo "echo command number $i"; i=$((i+1)); done > "$H/.simple_shell_history"
chmod 600 "$H/.simple_shell_history"

# Startup cost is measured as: launch + load history + exit on EOF.
best=999999
for run in 1 2 3 4 5; do
  start=$(python3 -c 'import time; print(int(time.time()*1000))')
  HOME="$H" "$BIN" < /dev/null
  end=$(python3 -c 'import time; print(int(time.time()*1000))')
  t=$((end - start))
  [ $t -lt $best ] && best=$t
done
rm -rf "$H"
echo "best startup: ${best} ms (limit ${LIMIT_MS} ms)"
[ $best -le $LIMIT_MS ] || { echo "FAIL"; exit 1; }
echo "pt02 OK"
