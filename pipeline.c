#include <stdbool.h>
#include "cache.h"
#include "riscv.h"
#include "types.h"
#include "utils.h"
#include "pipeline.h"
#include "stage_helpers.h"

uint64_t total_cycle_counter = 0;
uint64_t miss_count = 0;
uint64_t hit_count = 0;
uint64_t stall_counter = 0;
uint64_t branch_counter = 0;
uint64_t fwd_exex_counter = 0;
uint64_t fwd_exmem_counter = 0;
uint64_t mem_access_counter = 0;

simulator_config_t sim_config = {0};

#ifdef DEBUG_CYCLE
// 0x00000000 is not a valid opcode (it fills the empty bootstrap slots and
// memory past the program), and decode_instruction exits on it; print a bare
// newline instead, matching the reference traces
static void print_instruction(uint32_t bits)
{
  if (bits) decode_instruction(bits);
  else      putchar('\n');
}
#endif

///////////////////////////////////////////////////////////////////////////////

void bootstrap(pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p, regfile_t* regfile_p)
{
  // PC src must get the same value as the default PC value
  pwires_p->pc_src0 = regfile_p->PC;

  // empty bootstrap slots hold 0x00000000, matching the reference traces;
  // print_instruction() below handles printing them as blank lines
  pregs_p->ifid_preg.inp.instr.bits   = 0x00000000;
  pregs_p->ifid_preg.out.instr.bits   = 0x00000000;
  pregs_p->idex_preg.inp.instr.bits   = 0x00000000;
  pregs_p->idex_preg.out.instr.bits   = 0x00000000;
  pregs_p->exmem_preg.inp.instr.bits  = 0x00000000;
  pregs_p->exmem_preg.out.instr.bits  = 0x00000000;
  pregs_p->memwb_preg.inp.instr.bits  = 0x00000000;
  pregs_p->memwb_preg.out.instr.bits  = 0x00000000;
}

///////////////////////////
/// STAGE FUNCTIONALITY ///
///////////////////////////

/**
 * STAGE  : stage_fetch
 * output : ifid_reg_t
 **/ 
ifid_reg_t stage_fetch(pipeline_wires_t* pwires_p, regfile_t* regfile_p, Byte* memory_p)
{
  ifid_reg_t ifid_reg = {0};

  // IF-stage mux: pcsrc selects between the default "PC+4" path (pc_src0)
  // and a redirected target (pc_src1) set by stage_execute when a branch/jal
  // resolved during the *previous* cycle.
  regfile_p->PC = pwires_p->pcsrc ? pwires_p->pc_src1 : pwires_p->pc_src0;
  pwires_p->pcsrc = false;  // one-shot redirect, consumed here

  uint32_t instruction_bits = load(memory_p, regfile_p->PC, LENGTH_WORD);
  // 0x00000000 is not a valid RISC-V opcode; it shows up when the pipeline
  // fetches past the end of the loaded program (memory is zero-initialized).
  // decode_instruction() already special-cases this for printing, so we
  // mirror that here instead of trying to parse it as a real instruction -
  // it's just treated as an empty/bubble slot.
  if (instruction_bits != 0)
  {
    ifid_reg.instr = parse_instruction(instruction_bits);
  }
  ifid_reg.instr_addr = regfile_p->PC;

  // default "next PC" for the following cycle; stage_execute may override
  // this via pcsrc/pc_src1 if the instruction now entering EX is a taken
  // branch or a jal.
  pwires_p->pc_src0 = regfile_p->PC + 4;

  #ifdef DEBUG_CYCLE
  printf("[IF ]: Instruction [%08x]@[%08x]: ", instruction_bits, regfile_p->PC);
  print_instruction(instruction_bits);
  #endif
  return ifid_reg;
}

/**
 * STAGE  : stage_decode
 * output : idex_reg_t
 **/ 
idex_reg_t stage_decode(ifid_reg_t ifid_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  #ifdef DEBUG_CYCLE
  printf("[ID ]: Instruction [%08x]@[%08x]: ", ifid_reg.instr.bits, ifid_reg.instr_addr);
  print_instruction(ifid_reg.instr.bits);
  #endif

  Instruction instr = ifid_reg.instr;

  // gen_control fills in all the control signals for us; we then layer the
  // register-read/immediate-generation ("datapath") work on top of it.
  idex_reg_t idex_reg = gen_control(instr);
  idex_reg.instr      = instr;
  idex_reg.instr_addr = ifid_reg.instr_addr;

  // figure out which register fields are meaningful for this instruction's
  // format so we read the right rs1/rs2/rd out of the union.
  uint32_t rs1, rs2, rd;
  bool use_rs1, use_rs2;
  gen_reg_fields(instr, &rs1, &rs2, &rd, &use_rs1, &use_rs2);

  // note: decode reads the regfile before writeback updates it this cycle,
  // so an instruction 3 behind its producer sees the pre-writeback value;
  // this matches the reference pipeline, which has no regfile-internal
  // forwarding (verified against ms2 random.trace)
  idex_reg.rs1_val = regfile_p->R[rs1];
  idex_reg.rs2_val = regfile_p->R[rs2];
  idex_reg.rd      = rd;
  idex_reg.rs1     = rs1;
  idex_reg.rs2     = rs2;
  idex_reg.use_rs1 = use_rs1;
  idex_reg.use_rs2 = use_rs2;
  idex_reg.imm     = gen_imm(instr);

  return idex_reg;
}

/**
 * STAGE  : stage_execute
 * output : exmem_reg_t
 **/ 
exmem_reg_t stage_execute(idex_reg_t idex_reg, pipeline_wires_t* pwires_p)
{
  #ifdef DEBUG_CYCLE
  printf("[EX ]: Instruction [%08x]@[%08x]: ", idex_reg.instr.bits, idex_reg.instr_addr);
  print_instruction(idex_reg.instr.bits);
  #endif

  exmem_reg_t exmem_reg = {0};
  exmem_reg.instr      = idex_reg.instr;
  exmem_reg.instr_addr = idex_reg.instr_addr;
  exmem_reg.reg_write  = idex_reg.reg_write;
  exmem_reg.mem_read   = idex_reg.mem_read;
  exmem_reg.mem_write  = idex_reg.mem_write;
  exmem_reg.mem_to_reg = idex_reg.mem_to_reg;
  exmem_reg.rd         = idex_reg.rd;
  exmem_reg.rs2_val    = idex_reg.rs2_val; // needed later for stores

  // EX-stage mux: ALUSrc selects the ALU's 2nd operand between rs2 (R-type,
  // branches) and the immediate (I-type/loads/stores/lui).
  uint32_t alu_control = gen_alu_control(idex_reg);
  uint32_t alu_inp2    = idex_reg.alu_src ? idex_reg.imm : idex_reg.rs2_val;
  exmem_reg.alu_result = execute_alu(idex_reg.rs1_val, alu_inp2, alu_control);

  // ms2: the branch decision/target is computed here (with forwarded operand
  // values), but it is only acted on when the instruction reaches mem - the
  // flush happens in cycle_pipeline once the outcome sits in the memwb register
  (void)pwires_p;
  if (idex_reg.branch)
  {
    if (gen_branch(idex_reg.instr, idex_reg.rs1_val, idex_reg.rs2_val))
    {
      exmem_reg.branch_taken  = true;
      exmem_reg.branch_target = idex_reg.instr_addr + idex_reg.imm;
    }
  }
  else if (idex_reg.jump)
  {
    exmem_reg.branch_taken  = true;
    exmem_reg.branch_target = idex_reg.instr_addr + idex_reg.imm;
    exmem_reg.alu_result    = idex_reg.instr_addr + 4; // jal writes ret. addr to rd
  }

  return exmem_reg;
}

/**
 * STAGE  : stage_mem
 * output : memwb_reg_t
 **/ 
memwb_reg_t stage_mem(exmem_reg_t exmem_reg, pipeline_wires_t* pwires_p, Byte* memory_p, Cache* cache_p)
{
  (void)pwires_p; // not needed in MS1
  (void)cache_p;  // cache isn't modeled until MS3

  #ifdef DEBUG_CYCLE
  printf("[MEM]: Instruction [%08x]@[%08x]: ", exmem_reg.instr.bits, exmem_reg.instr_addr);
  print_instruction(exmem_reg.instr.bits);
  #endif

  memwb_reg_t memwb_reg = {0};
  memwb_reg.instr      = exmem_reg.instr;
  memwb_reg.instr_addr = exmem_reg.instr_addr;
  memwb_reg.reg_write  = exmem_reg.reg_write;
  memwb_reg.mem_to_reg = exmem_reg.mem_to_reg;
  memwb_reg.rd         = exmem_reg.rd;
  memwb_reg.alu_result = exmem_reg.alu_result;
  memwb_reg.branch_taken  = exmem_reg.branch_taken;
  memwb_reg.branch_target = exmem_reg.branch_target;

  if (exmem_reg.mem_read || exmem_reg.mem_write)
  {
    mem_access_counter++;
    Instruction instr = exmem_reg.instr;

    // funct3 sits at the same bit offset for I-type (loads) and S-type
    // (stores), so instr.itype.funct3 correctly reads either one.
    Alignment align;
    switch(instr.itype.funct3 & 0x3)
    {
      case 0x0: align = LENGTH_BYTE;      break; // lb/lbu, sb
      case 0x1: align = LENGTH_HALF_WORD; break; // lh/lhu, sh
      default:  align = LENGTH_WORD;      break; // lw, sw
    }

    if (exmem_reg.mem_read)
    {
      uint32_t data = load(memory_p, exmem_reg.alu_result, align);
      switch(instr.itype.funct3)
      {
        case 0x0: data = sign_extend_number(data & 0xFF,   8);  break; // lb
        case 0x1: data = sign_extend_number(data & 0xFFFF, 16); break; // lh
        case 0x4: data = data & 0xFF;   break; // lbu
        case 0x5: data = data & 0xFFFF; break; // lhu
        default: break; // lw needs no extension
      }
      memwb_reg.mem_result = data;
    }

    if (exmem_reg.mem_write)
    {
      store(memory_p, exmem_reg.alu_result, align, exmem_reg.rs2_val);
    }
  }

  return memwb_reg;
}

/**
 * STAGE  : stage_writeback
 * output : nothing - The state of the register file may be changed
 **/ 
void stage_writeback(memwb_reg_t memwb_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  (void)pwires_p; // not needed in MS1

  #ifdef DEBUG_CYCLE
  printf("[WB ]: Instruction [%08x]@[%08x]: ", memwb_reg.instr.bits, memwb_reg.instr_addr);
  print_instruction(memwb_reg.instr.bits);
  #endif

  // WB-stage mux: mem_to_reg picks between the ALU result and the loaded
  // memory value. x0 is hardwired to zero, so writes targeting it are simply
  // dropped rather than ever landing in the register file.
  if (memwb_reg.reg_write && memwb_reg.rd != 0)
  {
    regfile_p->R[memwb_reg.rd] = memwb_reg.mem_to_reg ? memwb_reg.mem_result
                                                       : memwb_reg.alu_result;
  }
}

///////////////////////////////////////////////////////////////////////////////

/** 
 * excite the pipeline with one clock cycle
 **/
void cycle_pipeline(regfile_t* regfile_p, Byte* memory_p, Cache* cache_p, pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, bool* ecall_exit)
{
  #ifdef DEBUG_CYCLE
  printf("v==============");
  printf("Cycle Counter = %5ld", total_cycle_counter);
  printf("==============v\n\n");
  #endif

  // process each stage

  /* Output               |    Stage      |       Inputs  */
  pregs_p->ifid_preg.inp  = stage_fetch     (pwires_p, regfile_p, memory_p);

  // load-use hazard check, before the use instruction is decoded
  detect_hazard(pregs_p, pwires_p, regfile_p);

  pregs_p->idex_preg.inp  = stage_decode    (pregs_p->ifid_preg.out, pwires_p, regfile_p);

  // forwarding for the instruction about to execute
  gen_forward(pregs_p, pwires_p);

  pregs_p->exmem_preg.inp = stage_execute   (pregs_p->idex_preg.out, pwires_p);

  pregs_p->memwb_preg.inp = stage_mem       (pregs_p->exmem_preg.out, pwires_p, memory_p, cache_p);

                            stage_writeback (pregs_p->memwb_preg.out, pwires_p, regfile_p);

  if (pwires_p->stall)
  {
    // hold the stalled instruction in ifid so it decodes again next cycle
    // (pc was already rewritten in detect_hazard so if refetches next cycle)
    pregs_p->ifid_preg.inp = pregs_p->ifid_preg.out;
    // the freshly decoded copy still travels down the pipeline (it keeps its
    // instruction bits and source registers, so the forwarding unit fires on
    // it just like the reference), but every effect is squashed: it must not
    // write a register, touch memory, or resolve as a branch
    pregs_p->idex_preg.inp.reg_write = false;
    pregs_p->idex_preg.inp.mem_read  = false;
    pregs_p->idex_preg.inp.mem_write = false;
    pregs_p->idex_preg.inp.branch    = false;
    pregs_p->idex_preg.inp.jump      = false;
    pwires_p->stall = false;
  }

  // control hazard: branch/jal outcome now sits in the memwb register
  if (pregs_p->memwb_preg.inp.branch_taken)
  {
    pwires_p->pcsrc   = true;
    pwires_p->pc_src1 = pregs_p->memwb_preg.inp.branch_target;
    branch_counter++;

    // flush ifid, idex, exmem: nop the instr, drop all control, keep the addr
    ifid_reg_t  ifid_f  = {0};
    idex_reg_t  idex_f  = {0};
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

  // update all the output registers for the next cycle from the input registers in the current cycle
  pregs_p->ifid_preg.out  = pregs_p->ifid_preg.inp;
  pregs_p->idex_preg.out  = pregs_p->idex_preg.inp;
  pregs_p->exmem_preg.out = pregs_p->exmem_preg.inp;
  pregs_p->memwb_preg.out = pregs_p->memwb_preg.inp;

  /////////////////// NO CHANGES BELOW THIS ARE REQUIRED //////////////////////

  // increment the cycle
  total_cycle_counter++;

  #ifdef DEBUG_REG_TRACE
  print_register_trace(regfile_p);
  #endif

  /**
   * check ecall condition
   * To do this, the value stored in R[10] (a0 or x10) should be 10.
   * Hence, the ecall condition is checked by the existence of following
   * two instructions in sequence:
   * 1. <instr>  x10, <val1>, <val2> 
   * 2. ecall
   * 
   * The first instruction must write the value 10 to x10.
   * The second instruction is the ecall (opcode: 0x73)
   * 
   * The condition checks whether the R[10] value is 10 when the
   * `memwb_reg.instr.opcode` == 0x73 (to propagate the ecall)
   * 
   * If more functionality on ecall needs to be added, it can be done
   * by adding more conditions on the value of R[10]
   */
  if( (pregs_p->memwb_preg.out.instr.bits == 0x00000073) &&
      (regfile_p->R[10] == 10) )
  {
    *(ecall_exit) = true;
  }
}
