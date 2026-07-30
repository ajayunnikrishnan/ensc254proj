#!/usr/bin/env bash
set -euo pipefail

# run from the project root since make and riscv live there
cd "$(dirname "$0")/.."

make clean >/dev/null
make MS4=1 BP_MODE=1 >/dev/null

mkdir -p ms4_tests/out
OUTPUT="ms4_tests/out/branch_predictor_1bit.trace"

./riscv -s -f -c -e -v \
  ms4_tests/input/branch_predictor_1bit.input > "$OUTPUT"

cat "$OUTPUT"

grep -q "#Branch predictor   = 1-bit last outcome" "$OUTPUT"

grep -q "#Branch predictions =     8" "$OUTPUT"
grep -q "#BP correct         =     6" "$OUTPUT"
grep -q "#BP incorrect       =     2" "$OUTPUT"
grep -q "#BP accuracy        =  75.0%" "$OUTPUT"

echo
echo "1-bit branch predictor test: PASSED"
