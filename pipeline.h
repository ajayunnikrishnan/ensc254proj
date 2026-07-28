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
// MS4 PART 1 START
//  Separate counters for instruction-cache statistics.
extern uint64_t icache_hit_count;      // instruction cache hits
extern uint64_t icache_miss_count;     // instruction cache misses
extern uint64_t icache_access_counter; // instruction cache accesses
// MS4 PART 1 END

// MS4 PART 2 START
// Statistics for the one-bit branch predictor.
extern uint64_t branch_prediction_count;     // total number of branch predictions made
extern uint64_t branch_prediction_correct;   // number of correct branch predictions
extern uint64_t branch_prediction_incorrect; // number of incorrect branch predictions
// MS4 PART 2 END

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

  //  MS4 PART 2 START
  // Prediction made when this conditional branch was decoded.
  bool predicted_taken;      // Whether the branch was predicted to be taken or not.
  uint32_t predicted_target; // The predicted target address for the branch.
  // MS4 PART 2 END
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

  // MS4 PART 2 START
  bool is_branch;            // Whether the instruction is a conditional branch.
  bool is_jump;              // Whether the instruction is a jump instruction.
  bool predicted_taken;      // Whether the branch was predicted to be taken or not.
  uint32_t predicted_target; // The predicted target address for the branch.
                             // MS4 PART 2 END
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

  //  MS4 PART 2 START
  bool is_branch;            // Whether the instruction is a conditional branch.
  bool is_jump;              // Whether the instruction is a jump instruction.
  bool predicted_taken;      // Whether the branch was predicted to be taken or not.
  uint32_t predicted_target; // The predicted target address for the branch.
  //  MS4 PART 2 END
} memwb_reg_t;

///////////////////////////////////////////////////////////////////////////////
/// Register types with input and output variants for simulator
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  ifid_reg_t inp;
  ifid_reg_t out;
} ifid_reg_pair_t;

typedef struct
{
  idex_reg_t inp;
  idex_reg_t out;
} idex_reg_pair_t;

typedef struct
{
  exmem_reg_t inp;
  exmem_reg_t out;
} exmem_reg_pair_t;

typedef struct
{
  memwb_reg_t inp;
  memwb_reg_t out;
} memwb_reg_pair_t;

///////////////////////////////////////////////////////////////////////////////
/// Functional pipeline requirements
///////////////////////////////////////////////////////////////////////////////

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
} pipeline_wires_t;

///////////////////////////////////////////////////////////////////////////////
/// Function definitions for different stages
///////////////////////////////////////////////////////////////////////////////

ifid_reg_t stage_fetch(
    pipeline_wires_t *pwires_p,
    regfile_t *regfile_p,
    Byte *memory_p,
    // MS4: instruction cache used during the IF stage.
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

void cycle_pipeline(
    regfile_t *regfile_p,
    Byte *memory_p,
    Cache *dcache_p,
    // MS4: pass a second, independent cache to the pipeline.
    Cache *icache_p,
    pipeline_regs_t *pregs_p,
    pipeline_wires_t *pwires_p,
    bool *ecall_exit);

void bootstrap(
    pipeline_wires_t *pwires_p,
    pipeline_regs_t *pregs_p,
    regfile_t *regfile_p);

#endif
