#!/bin/bash
set -e

mkdir -p ./code/ms4/out

make clean
make

echo "========================================"
echo "MS4 Test 1: Basic I-cache hit/miss test"
echo "========================================"
./riscv -s -f -c -e -v ./code/ms4/input/icache_basic.input \
  | tee ./code/ms4/out/icache_basic.trace

grep -q "#I-cache accesses  =    10" ./code/ms4/out/icache_basic.trace
grep -q "#I-cache hits      =     9" ./code/ms4/out/icache_basic.trace
grep -q "#I-cache misses    =     1" ./code/ms4/out/icache_basic.trace
grep -q "#I-cache evictions =     0" ./code/ms4/out/icache_basic.trace

echo
echo "Basic I-cache test: PASSED"
echo
echo "========================================"
echo "MS4 Test 2: I-cache eviction test"
echo "========================================"
./riscv -s -f -c -e -v ./code/ms4/input/icache_eviction.input \
  | tee ./code/ms4/out/icache_eviction.trace

grep -q "#I-cache accesses  =  1109" ./code/ms4/out/icache_eviction.trace
grep -q "#I-cache hits      =  1039" ./code/ms4/out/icache_eviction.trace
grep -q "#I-cache misses    =    70" ./code/ms4/out/icache_eviction.trace
grep -q "#I-cache evictions =     6" ./code/ms4/out/icache_eviction.trace

echo
echo "I-cache eviction test: PASSED"
echo "All MS4 instruction-cache tests passed."
