#!/bin/sh
# Smoke test for the skeleton. Teammates add their own checks below.
set -e
BIN=./simple_shell

[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }

echo "" | $BIN
echo "PASS: shell starts and exits cleanly on EOF"
