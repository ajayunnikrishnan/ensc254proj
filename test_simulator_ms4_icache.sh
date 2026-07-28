#!/bin/bash
set -e
make clean
make
./riscv -s -f -c -e -v ./code/ms4/input/icache_basic.input | tee ./code/ms4/out/icache_basic.trace
