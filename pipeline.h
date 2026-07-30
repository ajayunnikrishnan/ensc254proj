#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include "config.h"
#include "types.h"
#include "cache.h"
#include <stdbool.h>

///////////////////////////////////////////////////////////////////////////////
/// Functionality
///////////////////////////////////////////////////////////////////////////////

extern simulator_config_t sim_config;
extern uint64_t miss_count;
extern uint64_t hit_count;
extern uint64_t total_cycle_counter;
extern uint64_t stall_counter;
extern uint64_t branch_counter;
extern uint64_t fwd_exex_counter;
extern uint64_t fwd_exmem_counter;
extern uint64_t mem_access_counter;

// ms4 part 1 start
// separate counters for the instruction cache stats
extern uint64_t icache_hit_count;      // instruction cache hits
extern uint64_t icache_miss_count;     // instruction cache misses
extern uint64_t icache_access_counter; // instruction cache accesses
// ms4 part 1 end

// ms4 part 2 start
// shared stats for the branch predictors
extern uint64_t branch_prediction_count;     // total number of branch predictions made
extern uint64_t branch_prediction_correct;   // number of correct branch predictions
extern uint64_t branch_prediction_incorrect; // number of incorrect branch predictions
// ms4 part 2 end

///////////////////////////////////////////////////////////////////////////////
/// RISC-V Pipeline Register Types
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  Instruction instr;
  uint32_t instr_addr;
} ifid_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t instr_addr;

  uint32_t rs1_val;
  uint32_t rs2_val;
  uint32_t imm;
  uint32_t rd;

  uint32_t rs1;
  uint32_t rs2;

  bool use_rs1;
  bool use_rs2;

  bool reg_write;
  bool mem_read;
  bool mem_write;
  bool mem_to_reg;
  bool alu_src;
  bool branch;
  bool jump;

  // ms4 part 2 start
  // prediction made when this branch was decoded
  bool predicted_taken;      // was this branch predicted taken
  uint32_t predicted_target; // predicted target address
  // ms4 part 2 end
} idex_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t instr_addr;

  uint32_t alu_result;
  uint32_t rs2_val;
  uint32_t rd;

  bool reg_write;
  bool mem_read;
  bool mem_write;
  bool mem_to_reg;

  bool branch_taken;
  uint32_t branch_target;

  // for ms1
  bool jump;
  bool branch;

  // ms4 part 2 start
  bool is_branch;            // is this a conditional branch
  bool is_jump;              // is this a jump
  bool predicted_taken;      // was this branch predicted taken
  uint32_t predicted_target; // predicted target address
  // ms4 part 2 end
} exmem_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t instr_addr;

  uint32_t alu_result;
  uint32_t mem_result;
  uint32_t rd;

  bool reg_write;
  bool mem_to_reg;

  bool branch_taken;
  uint32_t branch_target;

  // ms4 part 2 start
  bool is_branch;            // is this a conditional branch
  bool is_jump;              // is this a jump
  bool predicted_taken;      // was this branch predicted taken
  uint32_t predicted_target; // predicted target address
  // ms4 part 2 end
} memwb_reg_t;

typedef struct
{
  ifid_reg_t inp; ifid_reg_t out;
} ifid_reg_pair_t;

typedef struct
{
  idex_reg_t inp; idex_reg_t out;
} idex_reg_pair_t;

typedef struct
{
  exmem_reg_t inp; exmem_reg_t out;
} exmem_reg_pair_t;

typedef struct
{
  memwb_reg_t inp; memwb_reg_t out;
} memwb_reg_pair_t;

typedef struct
{
  ifid_reg_pair_t ifid_preg;
  idex_reg_pair_t idex_preg;
  exmem_reg_pair_t exmem_preg;
  memwb_reg_pair_t memwb_preg;
} pipeline_regs_t;

typedef struct
{
  bool pcsrc;

  uint32_t pc_src0;
  uint32_t pc_src1;

  bool stall;

  // added ms2 wires
  uint8_t   forwardA; 
  uint8_t   forwardB; 
  uint32_t  exmem_fwd_val;
  uint32_t  memwb_fwd_val;
} pipeline_wires_t;

///////////////////////////////////////////////////////////////////////////////
/// Function definitions for different stages
///////////////////////////////////////////////////////////////////////////////

ifid_reg_t stage_fetch(
    pipeline_wires_t *pwires_p,
    regfile_t *regfile_p,
    Byte *memory_p,
    // ms4: instruction cache used in the fetch stage
    Cache *icache_p);

idex_reg_t stage_decode(
    ifid_reg_t ifid_reg,
    pipeline_wires_t *pwires_p,
    regfile_t *regfile_p);

exmem_reg_t stage_execute(
    idex_reg_t idex_reg,
    pipeline_wires_t *pwires_p);

memwb_reg_t stage_mem(
    exmem_reg_t exmem_reg,
    pipeline_wires_t *pwires_p,
    Byte *memory,
    Cache *cache_p);

void stage_writeback(
    memwb_reg_t memwb_reg,
    pipeline_wires_t *pwires_p,
    regfile_t *regfile_p);

void bootstrap(pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p, regfile_t* regfile_p);

// keeps the original framework signature so this still links against the
// instructor's riscv.c, the ms4 icache lives inside pipeline.c and only
// turns on when ICACHE_ENABLE is defined
void cycle_pipeline(regfile_t* regfile_p, Byte* memory_p, Cache* cache_p, pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, bool* ecall_exit);

#ifdef ICACHE_ENABLE
// getter for the internal instruction cache so riscv.c can print its stats
Cache* pipeline_icache(void);
#endif

#endif // __PIPELINE_H__