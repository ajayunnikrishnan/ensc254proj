#!/usr/bin/env bash
set -euo pipefail

make clean >/dev/null
make BP_MODE=1 >/dev/null

mkdir -p code/ms4/out
OUTPUT="code/ms4/out/branch_predictor_1bit.trace"

./riscv -s -f -c -e -v \
  code/ms4/input/branch_predictor_1bit.input > "$OUTPUT"

cat "$OUTPUT"

grep -q "#Branch predictor   = 1-bit last outcome" "$OUTPUT"

grep -q "#Branch predictions =     8" "$OUTPUT"
grep -q "#BP correct         =     6" "$OUTPUT"
grep -q "#BP incorrect       =     2" "$OUTPUT"
grep -q "#BP accuracy        =  75.0%" "$OUTPUT"

echo
echo "1-bit branch predictor test: PASSED"
