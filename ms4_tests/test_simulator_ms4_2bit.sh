#!/usr/bin/env bash
set -euo pipefail

# run from the project root since make and riscv live there
cd "$(dirname "$0")/.."

make clean >/dev/null
make MS4=1 BP_MODE=2 >/dev/null

mkdir -p ms4_tests/out
OUTPUT="ms4_tests/out/branch_predictor_2bit.trace"

./riscv -s -f -c -e -v \
  ms4_tests/input/branch_predictor_1bit.input > "$OUTPUT"

cat "$OUTPUT"

grep -q "#Branch predictor   = 2-bit saturating counter" "$OUTPUT"
grep -q "#Branch predictions =     7" "$OUTPUT"
grep -q "#BP correct         =     4" "$OUTPUT"
grep -q "#BP incorrect       =     3" "$OUTPUT"
grep -q "#BP accuracy        =  57.1%" "$OUTPUT"

echo
echo "2-bit branch predictor test: PASSED"
