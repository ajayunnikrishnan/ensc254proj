#!/usr/bin/env bash
# runs every milestone test suite and prints pass or fail per suite
# config.h is never modified, each phase builds with -D__CONFIG_H__ which
# skips the header body and passes the milestone macros on the compiler
# command line instead
set -u
cd "$(dirname "$0")"

PASS=0
FAIL=0

# build_profile <-D flags>, clean build with the given macros, config.h
# stays untouched since -D__CONFIG_H__ makes its include guard skip it
build_profile() {
  make clean >/dev/null 2>&1
  if ! make PROFILE_FLAGS="-D__CONFIG_H__ $*" >/dev/null 2>&1; then
    echo "BUILD FAILED"
    exit 1
  fi
}

# run_suite <name> <command> <regex of expected output lines>
# a suite passes when nothing is left after filtering the expected lines
run_suite() {
  local name="$1" cmd="$2" filter="$3"
  local out
  out=$(eval "$cmd" 2>&1 | grep -vE "$filter" | grep -v '^[[:space:]]*$')
  if [ -z "$out" ]; then
    echo "PASS: $name"
    PASS=$((PASS + 1))
  else
    echo "FAIL: $name"
    echo "$out" | head -10
    FAIL=$((FAIL + 1))
  fi
}

mkdir -p code/ms1/out/R code/ms1/out/I code/ms1/out/LS \
         code/ms2/out/R code/ms2/out/I code/ms2/out/LS \
         code/ms3/out/LS ms4_tests/out

# ms1
build_profile -DDEBUG_REG_TRACE -DDEBUG_CYCLE -DMEM_LATENCY=0
run_suite "MS1 (5 tests)" "bash test_simulator_ms1.sh" '^diff '

# ms2
build_profile -DDEBUG_REG_TRACE -DDEBUG_CYCLE -DPRINT_STATS -DMEM_LATENCY=0
run_suite "MS2 set 1 (6 tests)" "bash test_simulator_ms2.sh" '^diff '

build_profile -DPRINT_STATS -DMEM_LATENCY=0
run_suite "MS2 extended (vec_xprod stats)" "bash test_simulator_ms2_extended.sh" '^diff '

# ms3
MS3_NOISE='^diff |Please make sure|If it takes more than'
build_profile -DDEBUG_REG_TRACE -DDEBUG_CYCLE -DPRINT_STATS -DMEM_LATENCY=100 \
              -DCACHE_ENABLE -DPRINT_CACHE_STATS -DPRINT_CACHE_TRACES
run_suite "MS3 cache_complete (4 tests)" "bash test_simulator_ms3.sh cache_complete" "$MS3_NOISE"

build_profile -DPRINT_STATS -DMEM_LATENCY=100 -DCACHE_ENABLE -DPRINT_CACHE_STATS
run_suite "MS3 cache_summary (vec_xprod)" "bash test_simulator_ms3.sh cache_summary" "$MS3_NOISE"

build_profile -DPRINT_STATS -DMEM_LATENCY=100 -DPRINT_CACHE_STATS
run_suite "MS3 no_cache (vec_xprod)" "bash test_simulator_ms3.sh no_cache" "$MS3_NOISE"

# ms4, these scripts rebuild themselves with make MS4=1 so nothing to
# configure here
if bash ms4_tests/test_all_ms4.sh >/dev/null 2>&1; then
  echo "PASS: MS4 (icache x2 + 1-bit BP + 2-bit BP)"
  PASS=$((PASS + 1))
else
  echo "FAIL: MS4 (rerun ./ms4_tests/test_all_ms4.sh for details)"
  FAIL=$((FAIL + 1))
fi
if ./ms4_tests/terminal_gui.sh --check >/dev/null 2>&1; then
  echo "PASS: MS4 terminal GUI self-check"
  PASS=$((PASS + 1))
else
  echo "FAIL: MS4 terminal GUI self-check"
  FAIL=$((FAIL + 1))
fi

# leave behind a normal build that matches whatever config.h says
make clean >/dev/null 2>&1 && make >/dev/null 2>&1

echo
echo "==============================="
echo "  $PASS suites passed, $FAIL failed"
echo "==============================="
exit $((FAIL > 0))
