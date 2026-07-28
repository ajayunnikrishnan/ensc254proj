#!/usr/bin/env bash
set -euo pipefail

echo "========================================"
echo "MS4 Part 1: Instruction cache"
echo "========================================"
chmod +x test_simulator_ms4.sh
./test_simulator_ms4.sh

echo
echo "========================================"
echo "MS4 Part 2: 1-bit branch predictor"
echo "========================================"
chmod +x test_simulator_ms4_branch_predictor.sh
./test_simulator_ms4_branch_predictor.sh

echo
echo "========================================"
echo "MS4 Part 3: 2-bit branch predictor"
echo "========================================"
chmod +x test_simulator_ms4_2bit.sh
./test_simulator_ms4_2bit.sh

echo
echo "========================================"
echo "ALL THREE MS4 TESTS PASSED"
echo "========================================"
