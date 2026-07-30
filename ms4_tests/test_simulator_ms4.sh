#!/bin/bash
set -e

# run from the project root since make and riscv live there
cd "$(dirname "$0")/.."

mkdir -p ./ms4_tests/out

make clean
make MS4=1

echo "========================================"
echo "MS4 Test 1: Basic I-cache hit/miss test"
echo "========================================"
./riscv -s -f -c -e -v ./ms4_tests/input/icache_basic.input \
  | tee ./ms4_tests/out/icache_basic.trace

grep -q "#I-cache accesses  =    10" ./ms4_tests/out/icache_basic.trace
grep -q "#I-cache hits      =     9" ./ms4_tests/out/icache_basic.trace
grep -q "#I-cache misses    =     1" ./ms4_tests/out/icache_basic.trace
grep -q "#I-cache evictions =     0" ./ms4_tests/out/icache_basic.trace

echo
echo "Basic I-cache test: PASSED"
echo
echo "========================================"
echo "MS4 Test 2: I-cache eviction test"
echo "========================================"
./riscv -s -f -c -e -v ./ms4_tests/input/icache_eviction.input \
  | tee ./ms4_tests/out/icache_eviction.trace

grep -q "#I-cache accesses  =  1109" ./ms4_tests/out/icache_eviction.trace
grep -q "#I-cache hits      =  1039" ./ms4_tests/out/icache_eviction.trace
grep -q "#I-cache misses    =    70" ./ms4_tests/out/icache_eviction.trace
grep -q "#I-cache evictions =     6" ./ms4_tests/out/icache_eviction.trace

echo
echo "I-cache eviction test: PASSED"
echo "All MS4 instruction-cache tests passed."
