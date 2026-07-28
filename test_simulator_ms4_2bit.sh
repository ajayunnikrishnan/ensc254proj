#!/usr/bin/env bash
set -euo pipefail

make clean >/dev/null
make BP_MODE=2 >/dev/null

mkdir -p code/ms4/out
OUTPUT="code/ms4/out/branch_predictor_2bit.trace"

./riscv -s -f -c -e -v \
  code/ms4/input/branch_predictor_1bit.input > "$OUTPUT"

cat "$OUTPUT"

grep -q "#Branch predictor   = 2-bit saturating counter" "$OUTPUT"
grep -q "#Branch predictions =     7" "$OUTPUT"
grep -q "#BP correct         =     4" "$OUTPUT"
grep -q "#BP incorrect       =     3" "$OUTPUT"
grep -q "#BP accuracy        =  57.1%" "$OUTPUT"

echo
echo "2-bit branch predictor test: PASSED"
