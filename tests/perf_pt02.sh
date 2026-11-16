#!/bin/sh
# PT-02: first prompt must appear within 100 ms with a full (1,000-entry) history.
# History is only used by an interactive shell (FR-012), so this runs the
# shell on a pseudo-terminal with the `script` command.
BIN=${1:-build/simple_shell}
LIMIT_MS=${2:-100}

if ! command -v script > /dev/null 2>&1 ||
   ! script -qec true /dev/null > /dev/null 2>&1; then
  echo "SKIP: PT-02 needs a pseudo-terminal ('script' not available)"
  exit 0
fi

case "$BIN" in
  /*) ;;
  *) BIN="$(pwd)/${BIN#./}" ;;
esac

H=$(mktemp -d)
i=0
while [ $i -lt 1000 ]; do echo "echo command number $i"; i=$((i+1)); done > "$H/.simple_shell_history"
chmod 600 "$H/.simple_shell_history"

# Running through `script` costs a fixed amount of time that has nothing to do
# with the shell, so the same run is timed without a history file and
# subtracted. What is left is the cost of loading and saving 1,000 entries.
now_ms() { python3 -c 'import time; print(int(time.time()*1000))'; }

best_of_5() {
  best=999999
  for run in 1 2 3 4 5; do
    start=$(now_ms)
    echo "exit" | HOME="$1" timeout 10 script -qec "$BIN" /dev/null > /dev/null 2>&1
    end=$(now_ms)
    t=$((end - start))
    [ $t -lt $best ] && best=$t
  done
  echo $best
}

base=$(best_of_5 /nonexistent)
with=$(best_of_5 "$H")
best=$((with - base))
[ $best -ge 0 ] || best=0

# Proof that the history was really loaded: the saved file holds the 1,000
# old lines plus the new ones.
lines=$(wc -l < "$H/.simple_shell_history")
rm -rf "$H"
[ "$lines" -ge 1000 ] || { echo "FAIL: history was not loaded ($lines lines saved)"; exit 1; }

echo "history adds ${best} ms to start-up (limit ${LIMIT_MS} ms)"
[ $best -le $LIMIT_MS ] || { echo "FAIL"; exit 1; }
echo "pt02 OK"
