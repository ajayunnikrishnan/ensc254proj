#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "cache.h"
#include "riscv.h"
#include "types.h"
#include "utils.h"
#include "pipeline.h"

// alu control codes
#define ALU_ADD  0x0
#define ALU_SUB  0x1
#define ALU_MUL  0x2
#define ALU_AND  0x3
#define ALU_OR   0x4
#define ALU_XOR  0x5
#define ALU_SLL  0x6
#define ALU_SRL  0x7
#define ALU_SRA  0x8
#define ALU_SLT  0x9
#define ALU_SLTU 0xA
#define ALU_MULH 0xB
#define ALU_DIV  0xC
#define ALU_REM  0xD

// global counters
uint64_t total_cycle_counter = 0;
uint64_t miss_count = 0;
uint64_t hit_count = 0;
uint64_t stall_counter = 0;
uint64_t branch_counter = 0;
uint64_t fwd_exex_counter = 0;
uint64_t fwd_exmem_counter = 0;
uint64_t mem_access_counter = 0;

// ms4 counters
uint64_t icache_hit_count = 0;      
uint64_t icache_miss_count = 0;     
uint64_t icache_access_counter = 0; 
uint64_t branch_prediction_count = 0;     
uint64_t branch_prediction_correct = 0;   
uint64_t branch_prediction_incorrect = 0; 

simulator_config_t sim_config = {0};

#if BRANCH_PREDICTOR_MODE == 1
typedef struct {
  bool valid;                
  uint32_t branch_pc;        
  bool last_taken;           
} one_bit_predictor_entry_t; 
static one_bit_predictor_entry_t branch_predictor[BRANCH_PREDICTOR_ENTRIES] = {0}; 
#endif

#if BRANCH_PREDICTOR_MODE == 2
typedef struct {
  bool valid;
  uint32_t branch_pc;
  uint8_t state;
} two_bit_predictor_entry_t;
static two_bit_predictor_entry_t two_bit_branch_predictor[BRANCH_PREDICTOR_ENTRIES] = {0};
#endif

#if BRANCH_PREDICTOR_MODE != 0
static uint32_t branch_predictor_index(uint32_t branch_pc) {
  return (branch_pc >> 2) % BRANCH_PREDICTOR_ENTRIES;
}

static bool predict_branch(uint32_t branch_pc) {
  uint32_t index = branch_predictor_index(branch_pc);
#if BRANCH_PREDICTOR_MODE == 2
  two_bit_predictor_entry_t *entry = &two_bit_branch_predictor[index];
  if (!entry->valid || entry->branch_pc != branch_pc) return TWO_BIT_INITIAL_STATE >= 2;
  return entry->state >= 2;
#else
  one_bit_predictor_entry_t *entry = &branch_predictor[index];
  return entry->valid && entry->branch_pc == branch_pc && entry->last_taken;
#endif
}

static void update_branch_predictor(uint32_t branch_pc, bool actual_taken) {
  uint32_t index = branch_predictor_index(branch_pc);
#if BRANCH_PREDICTOR_MODE == 2
  two_bit_predictor_entry_t *entry = &two_bit_branch_predictor[index];
  if (!entry->valid || entry->branch_pc != branch_pc) {
    entry->valid = true;
    entry->branch_pc = branch_pc;
    entry->state = TWO_BIT_INITIAL_STATE;
  }
  if (actual_taken) {
    if (entry->state < 3) entry->state++;
  } else {
    if (entry->state > 0) entry->state--;
  }
#else
  branch_predictor[index].valid = true;
  branch_predictor[index].branch_pc = branch_pc;
  branch_predictor[index].last_taken = actual_taken;
#endif
}
#endif

#ifdef DEBUG_CYCLE
static void print_instruction(uint32_t bits) {
  if (bits) decode_instruction(bits);
  else putchar('\n');
}
#endif

void print_register_trace(regfile_t* regfile_p) {
  for (uint8_t i = 0; i < 8; i++) {
    for (uint8_t j = 0; j < 4; j++) {
      printf("r%2d=%08x ", i * 4 + j, regfile_p->R[i * 4 + j]);
    }
    printf("\n");
  }
  printf("\n");
}

///////////////////////////////////////////////////////////////////////////////
/// CORE HELPER FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

idex_reg_t gen_control(Instruction instr) {
    idex_reg_t idex = {0};
    uint32_t op = instr.opcode;

    if (op == 0x33) { // r-type
        idex.reg_write = true;
    } else if (op == 0x13 || op == 0x03 || op == 0x67) { // i-type, loads, jalr
        idex.reg_write = true;
        idex.alu_src = true;
        if (op == 0x03) {
            idex.mem_read = true;
            idex.mem_to_reg = true;
        }
    } else if (op == 0x23) { // s-type
        idex.alu_src = true;
        idex.mem_write = true;
    } else if (op == 0x63) { // b-type
        idex.branch = true;
    } else if (op == 0x6F) { // jal
        idex.jump = true;
        idex.reg_write = true;
    } else if (op == 0x37) { // lui
        idex.reg_write = true;
        idex.alu_src = true;
    }
    return idex;
}

uint32_t gen_imm(Instruction instr) {
    uint32_t op = instr.opcode;
    if (op == 0x13 || op == 0x03 || op == 0x67) return sign_extend_number(instr.itype.imm, 12);
    if (op == 0x23) return sign_extend_number(get_store_offset(instr), 12);
    if (op == 0x63) return sign_extend_number(get_branch_offset(instr), 13);
    if (op == 0x6F) return sign_extend_number(get_jump_offset(instr), 21);
    if (op == 0x37) return instr.utype.imm << 12;
    return 0;
}

uint32_t execute_alu(uint32_t rs1, uint32_t rs2, uint32_t alu_control) {
    switch(alu_control) {
        case ALU_ADD:  return rs1 + rs2;
        case ALU_SUB:  return rs1 - rs2;
        case ALU_MUL:  return rs1 * rs2; // lower 32 bits of the product
        case ALU_SLL:  return rs1 << (rs2 & 0x1F);
        case ALU_SLT:  return (int32_t)rs1 < (int32_t)rs2 ? 1 : 0;
        case ALU_SLTU: return rs1 < rs2 ? 1 : 0;
        case ALU_XOR:  return rs1 ^ rs2;
        case ALU_SRL:  return rs1 >> (rs2 & 0x1F);
        case ALU_SRA:  return (int32_t)rs1 >> (rs2 & 0x1F);
        case ALU_OR:   return rs1 | rs2;
        case ALU_AND:  return rs1 & rs2;
        case ALU_MULH: // upper 32 bits of signed 64-bit product
            return (uint32_t)((((int64_t)(int32_t)rs1) * ((int64_t)(int32_t)rs2)) >> 32);
        case ALU_DIV:  // signed division, guard divide by zero
            if (rs2 == 0) return 0xFFFFFFFF;
            if (rs1 == 0x80000000 && rs2 == 0xFFFFFFFF) return 0x80000000; // overflow case
            return (uint32_t)(((int32_t)rs1) / ((int32_t)rs2));
        case ALU_REM:  // signed remainder
            if (rs2 == 0) return rs1;
            if (rs1 == 0x80000000 && rs2 == 0xFFFFFFFF) return 0;
            return (uint32_t)(((int32_t)rs1) % ((int32_t)rs2));
        default: return 0;
    }
}

uint32_t gen_alu_control(idex_reg_t idex_reg) {
  uint32_t alu_control = ALU_ADD;
  Instruction instr = idex_reg.instr;

  switch(instr.opcode) {
    case 0x33: 
      switch(instr.rtype.funct3) {
        case 0x0:
          if (instr.rtype.funct7 == 0x01)      alu_control = ALU_MUL; // mul
          else if (instr.rtype.funct7 == 0x20) alu_control = ALU_SUB; // sub
          else                                 alu_control = ALU_ADD; // add
          break;
        case 0x1: alu_control = (instr.rtype.funct7 == 0x01) ? ALU_MULH : ALU_SLL; break; // mulh/sll
        case 0x2: alu_control = ALU_SLT; break;
        case 0x3: alu_control = ALU_SLTU; break;
        case 0x4: alu_control = (instr.rtype.funct7 == 0x01) ? ALU_DIV : ALU_XOR; break; // div/xor
        case 0x5: alu_control = (instr.rtype.funct7 == 0x20) ? ALU_SRA : ALU_SRL; break;
        case 0x6: alu_control = (instr.rtype.funct7 == 0x01) ? ALU_REM : ALU_OR; break; // rem/or
        case 0x7: alu_control = ALU_AND; break;
      }
      break;
    case 0x13: 
      switch(instr.itype.funct3) {
        case 0x0: alu_control = ALU_ADD; break;
        case 0x1: alu_control = ALU_SLL; break;
        case 0x2: alu_control = ALU_SLT; break;
        case 0x3: alu_control = ALU_SLTU; break;
        case 0x4: alu_control = ALU_XOR; break;
        case 0x5: alu_control = (instr.itype.imm & 0x400) ? ALU_SRA : ALU_SRL; break;
        case 0x6: alu_control = ALU_OR; break;
        case 0x7: alu_control = ALU_AND; break;
      }
      break;
    case 0x63: alu_control = ALU_SUB; break;
    default: alu_control = ALU_ADD; break;
  }
  return alu_control;
}

bool gen_branch(Instruction instr, uint32_t rs1_val, uint32_t rs2_val) {
  bool taken = false;
  switch(instr.sbtype.funct3) {
    case 0x0: taken = (rs1_val == rs2_val); break;
    case 0x1: taken = (rs1_val != rs2_val); break;
    case 0x4: taken = ((int32_t)rs1_val < (int32_t)rs2_val); break;
    case 0x5: taken = ((int32_t)rs1_val >= (int32_t)rs2_val); break;
    case 0x6: taken = (rs1_val < rs2_val); break;
    case 0x7: taken = (rs1_val >= rs2_val); break;
    default:  taken = false; break;
  }
  return taken;
}

void gen_forward(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p) {
  idex_reg_t* idex = &pregs_p->idex_preg.out;
  exmem_reg_t* exmem = &pregs_p->exmem_preg.out;
  memwb_reg_t* memwb = &pregs_p->memwb_preg.out;

  pwires_p->forwardA = 0;
  pwires_p->forwardB = 0;
  
  pwires_p->exmem_fwd_val = exmem->alu_result;
  pwires_p->memwb_fwd_val = memwb->mem_to_reg ? memwb->mem_result : memwb->alu_result;

  if (exmem->reg_write && exmem->rd != 0) {
      if (idex->use_rs1 && exmem->rd == idex->rs1) {
          pwires_p->forwardA = 1;
          fwd_exex_counter++;
          #ifdef DEBUG_CYCLE
          printf("[FWD]: Resolving EX hazard on rs1: x%d\n", idex->rs1);
          #endif
      }
      if (idex->use_rs2 && exmem->rd == idex->rs2) {
          pwires_p->forwardB = 1;
          fwd_exex_counter++;
          #ifdef DEBUG_CYCLE
          printf("[FWD]: Resolving EX hazard on rs2: x%d\n", idex->rs2);
          #endif
      }
  }

  if (memwb->reg_write && memwb->rd != 0) {
      if (idex->use_rs1 && memwb->rd == idex->rs1 &&
          !(exmem->reg_write && exmem->rd != 0 && exmem->rd == idex->rs1)) {
          pwires_p->forwardA = 2;
          fwd_exmem_counter++;
          #ifdef DEBUG_CYCLE
          printf("[FWD]: Resolving MEM hazard on rs1: x%d\n", idex->rs1);
          #endif
      }
      if (idex->use_rs2 && memwb->rd == idex->rs2 &&
          !(exmem->reg_write && exmem->rd != 0 && exmem->rd == idex->rs2)) {
          pwires_p->forwardB = 2;
          fwd_exmem_counter++;
          #ifdef DEBUG_CYCLE
          printf("[FWD]: Resolving MEM hazard on rs2: x%d\n", idex->rs2);
          #endif
      }
  }
}

void detect_hazard(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, regfile_t* regfile_p) {
  idex_reg_t* idex = &pregs_p->idex_preg.out;
  ifid_reg_t* ifid = &pregs_p->ifid_preg.out;

  pwires_p->stall = false;

  if (idex->mem_read && idex->rd != 0) {
      uint32_t op = ifid->instr.opcode;
      bool uses_rs1 = false, uses_rs2 = false;
      uint32_t rs1 = 0, rs2 = 0;

      if (op == 0x33 || op == 0x63 || op == 0x23) { 
          uses_rs1 = true; uses_rs2 = true;
          rs1 = ifid->instr.rtype.rs1; rs2 = ifid->instr.rtype.rs2;
      } else if (op == 0x13 || op == 0x03 || op == 0x67) { 
          uses_rs1 = true;
          rs1 = ifid->instr.itype.rs1;
      }

      if ((uses_rs1 && idex->rd == rs1) || (uses_rs2 && idex->rd == rs2)) {
          // load-use hazard, refetch the instruction we just fetched
          pwires_p->stall = true;
          pwires_p->pc_src0 = regfile_p->PC;
          stall_counter++;
          #ifdef DEBUG_CYCLE
          printf("[HZD]: Stalling and rewriting PC: 0x%08x\n", regfile_p->PC);
          #endif
      }
  }
}

///////////////////////////////////////////////////////////////////////////////
/// PIPELINE STAGES
///////////////////////////////////////////////////////////////////////////////

void bootstrap(pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p, regfile_t* regfile_p) {
  pwires_p->pc_src0 = regfile_p->PC;
  memset(pregs_p, 0, sizeof(pipeline_regs_t));
}

ifid_reg_t stage_fetch(pipeline_wires_t* pwires_p, regfile_t* regfile_p, Byte* memory_p, Cache* icache_p) {
  ifid_reg_t ifid_reg = {0};

  regfile_p->PC = pwires_p->pcsrc ? pwires_p->pc_src1 : pwires_p->pc_src0;
  pwires_p->pcsrc = false;

#ifdef ICACHE_ENABLE
  if (sim_config.cache_en) {
    result icache_result = operateCache((unsigned long long)regfile_p->PC, icache_p);
    int icache_latency = 0;
    icache_access_counter++;
    if (icache_result.status == CACHE_HIT) {
      icache_hit_count++;
      icache_latency = CACHE_HIT_LATENCY;
    } else {
      icache_miss_count++;
      icache_latency = (icache_result.status == CACHE_MISS) ? CACHE_MISS_LATENCY : CACHE_OTHER_LATENCY;
    }
    if (icache_latency > 0) {
      total_cycle_counter += (uint64_t)(icache_latency - 1);
    }
  }
#else
  (void)icache_p;
#endif

  uint32_t instruction_bits = load(memory_p, regfile_p->PC, LENGTH_WORD);

  if (instruction_bits != 0) {
    ifid_reg.instr = parse_instruction(instruction_bits);
  }

  ifid_reg.instr_addr = regfile_p->PC;
  pwires_p->pc_src0 = regfile_p->PC + 4;

#ifdef DEBUG_CYCLE
  printf("[IF ]: Instruction [%08x]@[%08x]: ", instruction_bits, regfile_p->PC);
  print_instruction(instruction_bits);
#endif

  return ifid_reg;
}

idex_reg_t stage_decode(ifid_reg_t ifid_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p) {
#ifdef DEBUG_CYCLE
  printf("[ID ]: Instruction [%08x]@[%08x]: ", ifid_reg.instr.bits, ifid_reg.instr_addr);
  print_instruction(ifid_reg.instr.bits);
#endif

  Instruction instr = ifid_reg.instr;
  idex_reg_t idex_reg = gen_control(instr);

  idex_reg.instr = instr;
  idex_reg.instr_addr = ifid_reg.instr_addr;

  uint32_t rs1 = 0, rs2 = 0, rd = 0;
  bool use_rs1 = false, use_rs2 = false;

  uint32_t op = instr.opcode;
  if (op == 0x33 || op == 0x63 || op == 0x23) {
      use_rs1 = true; use_rs2 = true;
      rs1 = (op == 0x23) ? instr.stype.rs1 : ((op == 0x63) ? instr.sbtype.rs1 : instr.rtype.rs1);
      rs2 = (op == 0x23) ? instr.stype.rs2 : ((op == 0x63) ? instr.sbtype.rs2 : instr.rtype.rs2);
      rd = (op == 0x33) ? instr.rtype.rd : 0;
  } else if (op == 0x13 || op == 0x03 || op == 0x67) {
      use_rs1 = true;
      rs1 = instr.itype.rs1;
      rd = instr.itype.rd;
  } else if (op == 0x37 || op == 0x6F) {
      rd = (op == 0x37) ? instr.utype.rd : instr.ujtype.rd;
  }

  idex_reg.rs1_val = regfile_p->R[rs1];
  idex_reg.rs2_val = regfile_p->R[rs2];
  idex_reg.rd = rd;
  idex_reg.rs1 = rs1;
  idex_reg.rs2 = rs2;
  idex_reg.use_rs1 = use_rs1;
  idex_reg.use_rs2 = use_rs2;
  idex_reg.imm = gen_imm(instr);

#if BRANCH_PREDICTOR_MODE != 0
  if (idex_reg.branch) {
    idex_reg.predicted_taken = predict_branch(idex_reg.instr_addr);
    idex_reg.predicted_target = idex_reg.instr_addr + idex_reg.imm;
    branch_prediction_count++;

    if (idex_reg.predicted_taken) {
      pwires_p->pcsrc = true;
      pwires_p->pc_src1 = idex_reg.predicted_target;
    }
  }
#endif

  return idex_reg;
}

exmem_reg_t stage_execute(idex_reg_t idex_reg, pipeline_wires_t* pwires_p) {
#ifdef DEBUG_CYCLE
  printf("[EX ]: Instruction [%08x]@[%08x]: ", idex_reg.instr.bits, idex_reg.instr_addr);
  print_instruction(idex_reg.instr.bits);
#endif

  exmem_reg_t exmem_reg = {0};
  exmem_reg.instr = idex_reg.instr;
  exmem_reg.instr_addr = idex_reg.instr_addr;
  exmem_reg.reg_write = idex_reg.reg_write;
  exmem_reg.mem_read = idex_reg.mem_read;
  exmem_reg.mem_write = idex_reg.mem_write;
  exmem_reg.mem_to_reg = idex_reg.mem_to_reg;
  exmem_reg.rd = idex_reg.rd;

  // apply the forwarding muxes
  uint32_t val_rs1 = idex_reg.rs1_val;
  if (pwires_p->forwardA == 1) val_rs1 = pwires_p->exmem_fwd_val;
  else if (pwires_p->forwardA == 2) val_rs1 = pwires_p->memwb_fwd_val;

  uint32_t val_rs2 = idex_reg.rs2_val;
  if (pwires_p->forwardB == 1) val_rs2 = pwires_p->exmem_fwd_val;
  else if (pwires_p->forwardB == 2) val_rs2 = pwires_p->memwb_fwd_val;

  uint32_t alu_control = gen_alu_control(idex_reg);
  uint32_t alu_inp2 = idex_reg.alu_src ? idex_reg.imm : val_rs2;
  
  exmem_reg.alu_result = execute_alu(val_rs1, alu_inp2, alu_control);
  exmem_reg.rs2_val = val_rs2; // stores need the forwarded rs2 value in mem

  exmem_reg.is_branch = idex_reg.branch;
  exmem_reg.is_jump = idex_reg.jump;
  exmem_reg.predicted_taken = idex_reg.predicted_taken;
  exmem_reg.predicted_target = idex_reg.predicted_target;
  exmem_reg.branch_target = idex_reg.instr_addr + idex_reg.imm;

  if (idex_reg.branch) {
    if (gen_branch(idex_reg.instr, val_rs1, val_rs2)) exmem_reg.branch_taken = true;
  } else if (idex_reg.jump) {
    exmem_reg.branch_taken = true;
    exmem_reg.alu_result = idex_reg.instr_addr + 4; // jal writes the return address to rd
  }

  return exmem_reg;
}

memwb_reg_t stage_mem(exmem_reg_t exmem_reg, pipeline_wires_t* pwires_p, Byte* memory_p, Cache* cache_p) {
  (void)pwires_p;

#ifdef DEBUG_CYCLE
  printf("[MEM]: Instruction [%08x]@[%08x]: ", exmem_reg.instr.bits, exmem_reg.instr_addr);
  print_instruction(exmem_reg.instr.bits);
#endif

  memwb_reg_t memwb_reg = {0};
  memwb_reg.instr = exmem_reg.instr;
  memwb_reg.instr_addr = exmem_reg.instr_addr;
  memwb_reg.reg_write = exmem_reg.reg_write;
  memwb_reg.mem_to_reg = exmem_reg.mem_to_reg;
  memwb_reg.rd = exmem_reg.rd;
  memwb_reg.alu_result = exmem_reg.alu_result;
  memwb_reg.branch_taken = exmem_reg.branch_taken;
  memwb_reg.branch_target = exmem_reg.branch_target;
  memwb_reg.is_branch = exmem_reg.is_branch;
  memwb_reg.is_jump = exmem_reg.is_jump;
  memwb_reg.predicted_taken = exmem_reg.predicted_taken;
  memwb_reg.predicted_target = exmem_reg.predicted_target;

  if (exmem_reg.mem_read || exmem_reg.mem_write) {
    mem_access_counter++;

#ifdef CACHE_ENABLE
    int cache_latency = processCacheOperation((unsigned long)exmem_reg.alu_result, cache_p);
    if (cache_latency > 0) {
      total_cycle_counter += (uint64_t)(cache_latency - 1);
    }

    #ifdef PRINT_CACHE_TRACES
    printf("[MEM]: Cache latency at addr: 0x%08x: %d cycles\n",
           exmem_reg.alu_result, cache_latency);
    #endif
#else
    if (MEM_LATENCY > 0) {
      total_cycle_counter += (uint64_t)(MEM_LATENCY - 1);
    }
#endif

    Instruction instr = exmem_reg.instr;
    Alignment align;

    switch (instr.itype.funct3 & 0x3) {
      case 0x0: align = LENGTH_BYTE; break;
      case 0x1: align = LENGTH_HALF_WORD; break;
      default: align = LENGTH_WORD; break;
    }

    if (exmem_reg.mem_read) {
      uint32_t data = load(memory_p, exmem_reg.alu_result, align);
      switch (instr.itype.funct3) {
        case 0x0: data = sign_extend_number(data & 0xFF, 8); break;
        case 0x1: data = sign_extend_number(data & 0xFFFF, 16); break;
        case 0x4: data = data & 0xFF; break;
        case 0x5: data = data & 0xFFFF; break;
        default: break;
      }
      memwb_reg.mem_result = data;
    }

    if (exmem_reg.mem_write) {
      store(memory_p, exmem_reg.alu_result, align, exmem_reg.rs2_val);
    }
  }

  return memwb_reg;
}

void stage_writeback(memwb_reg_t memwb_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p) {
  (void)pwires_p;

#ifdef DEBUG_CYCLE
  printf("[WB ]: Instruction [%08x]@[%08x]: ", memwb_reg.instr.bits, memwb_reg.instr_addr);
  print_instruction(memwb_reg.instr.bits);
#endif

  if (memwb_reg.reg_write && memwb_reg.rd != 0) {
    regfile_p->R[memwb_reg.rd] = memwb_reg.mem_to_reg ? memwb_reg.mem_result : memwb_reg.alu_result;
  }
}

///////////////////////////////////////////////////////////////////////////////

#ifdef ICACHE_ENABLE
// ms4: pipeline.c owns the instruction cache so cycle_pipeline keeps the
// original signature that the framework riscv.c expects
static Cache instruction_cache;
static bool instruction_cache_ready = false;

Cache* pipeline_icache(void) {
  if (!instruction_cache_ready) {
    cacheSetUp(&instruction_cache, "L1 Instruction Cache");
    instruction_cache_ready = true;
  }
  return &instruction_cache;
}
#endif

void cycle_pipeline(regfile_t* regfile_p, Byte* memory_p, Cache* cache_p, pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, bool* ecall_exit) {
#ifdef ICACHE_ENABLE
  Cache* icache_p = pipeline_icache();
#else
  Cache* icache_p = NULL;
#endif
  Cache* dcache_p = cache_p;
#ifdef DEBUG_CYCLE
  printf("v==============");
  printf("Cycle Counter = %5ld", total_cycle_counter);
  printf("==============v\n\n");
#endif

  pregs_p->ifid_preg.inp  = stage_fetch(pwires_p, regfile_p, memory_p, icache_p);
  
  // ms2: hazard detection stays off for ms1
  #ifdef PRINT_STATS
  detect_hazard(pregs_p, pwires_p, regfile_p);
  #endif
  
  pregs_p->idex_preg.inp  = stage_decode(pregs_p->ifid_preg.out, pwires_p, regfile_p);

#if BRANCH_PREDICTOR_MODE != 0
  if (pregs_p->idex_preg.inp.branch && pregs_p->idex_preg.inp.predicted_taken) {
    ifid_reg_t predicted_flush = {0};
    predicted_flush.instr.bits = 0x00000013;
    pregs_p->ifid_preg.inp = predicted_flush;
  }
#endif

  // ms2: forwarding stays off for ms1
  #ifdef PRINT_STATS
  gen_forward(pregs_p, pwires_p);
  #endif

  pregs_p->exmem_preg.inp = stage_execute(pregs_p->idex_preg.out, pwires_p);
  pregs_p->memwb_preg.inp = stage_mem(pregs_p->exmem_preg.out, pwires_p, memory_p, dcache_p);
  stage_writeback(pregs_p->memwb_preg.out, pwires_p, regfile_p);

  // ms2: load-use stall handling stays off for ms1
  #ifdef PRINT_STATS
  if (pwires_p->stall) {
    // pc was already rewritten in detect_hazard so fetch redoes the
    // same instruction next cycle
    pregs_p->ifid_preg.inp = pregs_p->ifid_preg.out; // keep the id instruction where it is

    // turn this decode into a bubble, keep the instruction bits since the
    // reference traces show them moving down the pipe but clear anything
    // that would have an effect
    pregs_p->idex_preg.inp.reg_write = false;
    pregs_p->idex_preg.inp.mem_read  = false;
    pregs_p->idex_preg.inp.mem_write = false;
    pregs_p->idex_preg.inp.branch    = false;
    pregs_p->idex_preg.inp.jump      = false;
    pwires_p->stall = false;
  }
  #endif

#if BRANCH_PREDICTOR_MODE != 0
  if (pregs_p->memwb_preg.inp.is_branch) {
    memwb_reg_t resolved = pregs_p->memwb_preg.inp;
    bool actual_taken = resolved.branch_taken;
    bool prediction_correct = resolved.predicted_taken == actual_taken;

    if (prediction_correct) branch_prediction_correct++;
    else branch_prediction_incorrect++;

    update_branch_predictor(resolved.instr_addr, actual_taken);
    if (actual_taken) branch_counter++;

    if (!prediction_correct) {
      pwires_p->pcsrc = true;
      pwires_p->pc_src1 = actual_taken ? resolved.branch_target : resolved.instr_addr + 4;

      memset(&pregs_p->ifid_preg.inp, 0, sizeof(ifid_reg_t));
      pregs_p->ifid_preg.inp.instr.bits = 0x00000013;
      memset(&pregs_p->idex_preg.inp, 0, sizeof(idex_reg_t));
      pregs_p->idex_preg.inp.instr.bits = 0x00000013;
      memset(&pregs_p->exmem_preg.inp, 0, sizeof(exmem_reg_t));
      pregs_p->exmem_preg.inp.instr.bits = 0x00000013;
    }
  } else if (pregs_p->memwb_preg.inp.is_jump) {
    pwires_p->pcsrc = true;
    pwires_p->pc_src1 = pregs_p->memwb_preg.inp.branch_target;
    branch_counter++;

    memset(&pregs_p->ifid_preg.inp, 0, sizeof(ifid_reg_t));
    pregs_p->ifid_preg.inp.instr.bits = 0x00000013;
    memset(&pregs_p->idex_preg.inp, 0, sizeof(idex_reg_t));
    pregs_p->idex_preg.inp.instr.bits = 0x00000013;
    memset(&pregs_p->exmem_preg.inp, 0, sizeof(exmem_reg_t));
    pregs_p->exmem_preg.inp.instr.bits = 0x00000013;
  }
#else
  // control hazard resolution for ms1 and ms2
  if (pregs_p->memwb_preg.inp.branch_taken) {
    pwires_p->pcsrc = true;
    pwires_p->pc_src1 = pregs_p->memwb_preg.inp.branch_target;

    branch_counter++;

    // ms2: flushing stays off for ms1 since ms1 just runs the
    // delay slot instructions
    #ifdef PRINT_STATS
    {
      // flush if, id and ex into nops but keep each stage's instruction
      // address like the reference traces do
      ifid_reg_t ifid_f = {0};
      idex_reg_t idex_f = {0};
      exmem_reg_t exmem_f = {0};

      ifid_f.instr.bits  = 0x00000013;
      idex_f.instr.bits  = 0x00000013;
      exmem_f.instr.bits = 0x00000013;

      ifid_f.instr_addr  = pregs_p->ifid_preg.inp.instr_addr;
      idex_f.instr_addr  = pregs_p->idex_preg.inp.instr_addr;
      exmem_f.instr_addr = pregs_p->exmem_preg.inp.instr_addr;

      pregs_p->ifid_preg.inp  = ifid_f;
      pregs_p->idex_preg.inp  = idex_f;
      pregs_p->exmem_preg.inp = exmem_f;

      #ifdef DEBUG_CYCLE
      printf("[CPL]: Pipeline Flushed\n");
      #endif
    }
    #endif
  }
#endif

  pregs_p->ifid_preg.out  = pregs_p->ifid_preg.inp;
  pregs_p->idex_preg.out  = pregs_p->idex_preg.inp;
  pregs_p->exmem_preg.out = pregs_p->exmem_preg.inp;
  pregs_p->memwb_preg.out = pregs_p->memwb_preg.inp;

  total_cycle_counter++;

#ifdef DEBUG_REG_TRACE
  print_register_trace(regfile_p);
#endif

  if ((pregs_p->memwb_preg.out.instr.bits == 0x00000073) && (regfile_p->R[10] == 10)) {
    *ecall_exit = true;
  }
}