#!/bin/sh
# SEC-T01: feed 1,000 random lines (up to 1 MB) to the ASan/UBSan build of
# the shell. Passes only with 0 sanitizer reports, no crash and no hang.
# External commands are disabled (empty PATH) and the corpus has no '/' or
# '.', so random lines cannot touch anything outside a temporary directory.
set -eu

shell_bin=$1     # sanitizer build of simple_shell
fuzz_bin=$2      # tests/fuzz.c sanitizer build
count=${3:-1000}
seed=${4:-0x5EC001}
chunk=100

case "$shell_bin" in /*) ;; *) shell_bin="$(pwd)/$shell_bin" ;; esac
case "$fuzz_bin" in /*) ;; *) fuzz_bin="$(pwd)/$fuzz_bin" ;; esac

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
mkdir "$tmpdir/run"

"$fuzz_bin" emit "$count" "$seed" > "$tmpdir/corpus"
lines=$(wc -l < "$tmpdir/corpus")
[ "$lines" -eq "$count" ] || { echo "FAIL: corpus has $lines lines"; exit 1; }

# Chunks, so an "exit" in one random line does not skip the rest.
split -l "$chunk" "$tmpdir/corpus" "$tmpdir/part."

reports=0
for part in "$tmpdir"/part.*; do
    set +e
    (cd "$tmpdir/run" &&
     HOME="$tmpdir/run" PATH=/nonexistent-fuzz-path \
     ASAN_OPTIONS=detect_leaks=1:exitcode=99 \
     UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=98 \
     timeout 120 "$shell_bin" < "$part" > /dev/null 2> "$tmpdir/err")
    status=$?
    set -e

    if grep -E 'AddressSanitizer|LeakSanitizer|runtime error' "$tmpdir/err" > /dev/null ||
       [ "$status" -eq 98 ] || [ "$status" -eq 99 ] || [ "$status" -eq 124 ]; then
        echo "FAIL: $(basename "$part") (status $status):"
        grep -E -A20 'Sanitizer|runtime error' "$tmpdir/err" | head -40
        cp "$part" "./fuzz-failure-$(basename "$part").txt" 2>/dev/null || true
        reports=$((reports + 1))
    fi
done

if [ "$reports" -ne 0 ]; then
    echo "fuzz_shell: $reports chunk(s) with sanitizer reports or crashes"
    exit 1
fi
echo "fuzz_shell: $count lines, 0 sanitizer reports OK"
