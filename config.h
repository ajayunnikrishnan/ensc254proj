#ifndef __CONFIG_H__
#define __CONFIG_H__

//  MS4 PART 1 START
// Enable the separate instruction cache and its statistics.
// These switches are the main controls for the MS4 feature.
#define PRINT_STATS
#define MEM_LATENCY 100

// Existing MS3 data-cache support
#define CACHE_ENABLE
#define PRINT_CACHE_STATS
// #define PRINT_CACHE_TRACES

// MS4 instruction-cache support
#define ICACHE_ENABLE
#define PRINT_ICACHE_STATS
// #define PRINT_ICACHE_TRACES
// MS4 PART 1 END

// MS4 PART 2 START
// Branch predictor selection. This may be overridden at build time:
//   make BP_MODE=1   one-bit last-outcome predictor
//   make BP_MODE=2   two-bit saturating-counter predictor
//   make BP_MODE=0   predictor disabled
#ifndef BRANCH_PREDICTOR_MODE
#define BRANCH_PREDICTOR_MODE 2
#endif

#define PRINT_BRANCH_PREDICTOR_STATS
// #define PRINT_BRANCH_PREDICTOR_TRACES
#define BRANCH_PREDICTOR_ENTRIES 64
// MS4 PART 2 END

// MS4 PART 3 START
//  Initial state for the two-bit predictor:
//  0 = strongly not taken, 1 = weakly not taken,
//  2 = weakly taken,       3 = strongly taken.
#define TWO_BIT_INITIAL_STATE 0

#if BRANCH_PREDICTOR_MODE < 0 || BRANCH_PREDICTOR_MODE > 2 // invalid mode
#error "BRANCH_PREDICTOR_MODE must be 0, 1, or 2"
#endif
// MS4 PART 3 END

// Optional verbose debugging
// #define DEBUG_REG_TRACE
// #define DEBUG_CYCLE

#endif // __CONFIG_H__
