#!/bin/bash
set -e

# run from the project root since make and riscv live there
cd "$(dirname "$0")/.."
make clean
make MS4=1
./riscv -s -f -c -e -v ./ms4_tests/input/icache_basic.input | tee ./ms4_tests/out/icache_basic.trace
