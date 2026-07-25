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
static void print_instruction(uint32_t bits)
{
  if (bits)
    decode_instruction(bits);
  else
    putchar('\n');
}
#endif

///////////////////////////////////////////////////////////////////////////////

void bootstrap(
    pipeline_wires_t* pwires_p,
    pipeline_regs_t* pregs_p,
    regfile_t* regfile_p)
{
  pwires_p->pc_src0 = regfile_p->PC;

  pregs_p->ifid_preg.inp.instr.bits = 0x00000000;
  pregs_p->ifid_preg.out.instr.bits = 0x00000000;
  pregs_p->idex_preg.inp.instr.bits = 0x00000000;
  pregs_p->idex_preg.out.instr.bits = 0x00000000;
  pregs_p->exmem_preg.inp.instr.bits = 0x00000000;
  pregs_p->exmem_preg.out.instr.bits = 0x00000000;
  pregs_p->memwb_preg.inp.instr.bits = 0x00000000;
  pregs_p->memwb_preg.out.instr.bits = 0x00000000;
}

///////////////////////////
/// STAGE FUNCTIONALITY ///
///////////////////////////

ifid_reg_t stage_fetch(
    pipeline_wires_t* pwires_p,
    regfile_t* regfile_p,
    Byte* memory_p)
{
  ifid_reg_t ifid_reg = {0};

  regfile_p->PC =
      pwires_p->pcsrc
          ? pwires_p->pc_src1
          : pwires_p->pc_src0;

  pwires_p->pcsrc = false;

  uint32_t instruction_bits =
      load(memory_p, regfile_p->PC, LENGTH_WORD);

  if (instruction_bits != 0)
  {
    ifid_reg.instr =
        parse_instruction(instruction_bits);
  }

  ifid_reg.instr_addr = regfile_p->PC;

  pwires_p->pc_src0 = regfile_p->PC + 4;

#ifdef DEBUG_CYCLE
  printf(
      "[IF ]: Instruction [%08x]@[%08x]: ",
      instruction_bits,
      regfile_p->PC
  );

  print_instruction(instruction_bits);
#endif

  return ifid_reg;
}

idex_reg_t stage_decode(
    ifid_reg_t ifid_reg,
    pipeline_wires_t* pwires_p,
    regfile_t* regfile_p)
{
#ifdef DEBUG_CYCLE
  printf(
      "[ID ]: Instruction [%08x]@[%08x]: ",
      ifid_reg.instr.bits,
      ifid_reg.instr_addr
  );

  print_instruction(ifid_reg.instr.bits);
#endif

  Instruction instr = ifid_reg.instr;

  idex_reg_t idex_reg = gen_control(instr);

  idex_reg.instr = instr;
  idex_reg.instr_addr = ifid_reg.instr_addr;

  uint32_t rs1;
  uint32_t rs2;
  uint32_t rd;

  bool use_rs1;
  bool use_rs2;

  gen_reg_fields(
      instr,
      &rs1,
      &rs2,
      &rd,
      &use_rs1,
      &use_rs2
  );

  idex_reg.rs1_val = regfile_p->R[rs1];
  idex_reg.rs2_val = regfile_p->R[rs2];

  idex_reg.rd = rd;
  idex_reg.rs1 = rs1;
  idex_reg.rs2 = rs2;

  idex_reg.use_rs1 = use_rs1;
  idex_reg.use_rs2 = use_rs2;

  idex_reg.imm = gen_imm(instr);

  return idex_reg;
}

exmem_reg_t stage_execute(
    idex_reg_t idex_reg,
    pipeline_wires_t* pwires_p)
{
#ifdef DEBUG_CYCLE
  printf(
      "[EX ]: Instruction [%08x]@[%08x]: ",
      idex_reg.instr.bits,
      idex_reg.instr_addr
  );

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
  exmem_reg.rs2_val = idex_reg.rs2_val;

  uint32_t alu_control =
      gen_alu_control(idex_reg);

  uint32_t alu_inp2 =
      idex_reg.alu_src
          ? idex_reg.imm
          : idex_reg.rs2_val;

  exmem_reg.alu_result =
      execute_alu(
          idex_reg.rs1_val,
          alu_inp2,
          alu_control
      );

  (void)pwires_p;

  if (idex_reg.branch)
  {
    if (gen_branch(
            idex_reg.instr,
            idex_reg.rs1_val,
            idex_reg.rs2_val))
    {
      exmem_reg.branch_taken = true;

      exmem_reg.branch_target =
          idex_reg.instr_addr +
          idex_reg.imm;
    }
  }
  else if (idex_reg.jump)
  {
    exmem_reg.branch_taken = true;

    exmem_reg.branch_target =
        idex_reg.instr_addr +
        idex_reg.imm;

    exmem_reg.alu_result =
        idex_reg.instr_addr + 4;
  }

  return exmem_reg;
}

memwb_reg_t stage_mem(
    exmem_reg_t exmem_reg,
    pipeline_wires_t* pwires_p,
    Byte* memory_p,
    Cache* cache_p)
{
  (void)pwires_p;

#ifdef DEBUG_CYCLE
  printf(
      "[MEM]: Instruction [%08x]@[%08x]: ",
      exmem_reg.instr.bits,
      exmem_reg.instr_addr
  );

  print_instruction(exmem_reg.instr.bits);
#endif

  memwb_reg_t memwb_reg = {0};

  memwb_reg.instr = exmem_reg.instr;
  memwb_reg.instr_addr = exmem_reg.instr_addr;

  memwb_reg.reg_write = exmem_reg.reg_write;
  memwb_reg.mem_to_reg = exmem_reg.mem_to_reg;

  memwb_reg.rd = exmem_reg.rd;
  memwb_reg.alu_result = exmem_reg.alu_result;

  memwb_reg.branch_taken =
      exmem_reg.branch_taken;

  memwb_reg.branch_target =
      exmem_reg.branch_target;

  if (exmem_reg.mem_read ||
      exmem_reg.mem_write)
  {
    mem_access_counter++;

#ifdef CACHE_ENABLE
    int cache_latency =
        processCacheOperation(
            (unsigned long)exmem_reg.alu_result,
            cache_p
        );

    total_cycle_counter +=
        (uint64_t)(cache_latency - 1);

#ifdef PRINT_CACHE_TRACES
    printf(
        "[MEM]: Cache latency at addr: "
        "0x%08x: %d cycles\n",
        exmem_reg.alu_result,
        cache_latency
    );
#endif

#else
    total_cycle_counter +=
        (uint64_t)(MEM_LATENCY - 1);
#endif

    Instruction instr = exmem_reg.instr;

    Alignment align;

    switch (instr.itype.funct3 & 0x3)
    {
      case 0x0:
        align = LENGTH_BYTE;
        break;

      case 0x1:
        align = LENGTH_HALF_WORD;
        break;

      default:
        align = LENGTH_WORD;
        break;
    }

    if (exmem_reg.mem_read)
    {
      uint32_t data =
          load(
              memory_p,
              exmem_reg.alu_result,
              align
          );

      switch (instr.itype.funct3)
      {
        case 0x0:
          data = sign_extend_number(
              data & 0xFF,
              8
          );
          break;

        case 0x1:
          data = sign_extend_number(
              data & 0xFFFF,
              16
          );
          break;

        case 0x4:
          data = data & 0xFF;
          break;

        case 0x5:
          data = data & 0xFFFF;
          break;

        default:
          break;
      }

      memwb_reg.mem_result = data;
    }

    if (exmem_reg.mem_write)
    {
      store(
          memory_p,
          exmem_reg.alu_result,
          align,
          exmem_reg.rs2_val
      );
    }
  }

  return memwb_reg;
}

void stage_writeback(
    memwb_reg_t memwb_reg,
    pipeline_wires_t* pwires_p,
    regfile_t* regfile_p)
{
  (void)pwires_p;

#ifdef DEBUG_CYCLE
  printf(
      "[WB ]: Instruction [%08x]@[%08x]: ",
      memwb_reg.instr.bits,
      memwb_reg.instr_addr
  );

  print_instruction(memwb_reg.instr.bits);
#endif

  if (memwb_reg.reg_write &&
      memwb_reg.rd != 0)
  {
    regfile_p->R[memwb_reg.rd] =
        memwb_reg.mem_to_reg
            ? memwb_reg.mem_result
            : memwb_reg.alu_result;
  }
}

///////////////////////////////////////////////////////////////////////////////

void cycle_pipeline(
    regfile_t* regfile_p,
    Byte* memory_p,
    Cache* cache_p,
    pipeline_regs_t* pregs_p,
    pipeline_wires_t* pwires_p,
    bool* ecall_exit)
{
#ifdef DEBUG_CYCLE
  printf("v==============");
  printf(
      "Cycle Counter = %5ld",
      total_cycle_counter
  );
  printf("==============v\n\n");
#endif

  pregs_p->ifid_preg.inp =
      stage_fetch(
          pwires_p,
          regfile_p,
          memory_p
      );

  detect_hazard(
      pregs_p,
      pwires_p,
      regfile_p
  );

  pregs_p->idex_preg.inp =
      stage_decode(
          pregs_p->ifid_preg.out,
          pwires_p,
          regfile_p
      );

  gen_forward(
      pregs_p,
      pwires_p
  );

  pregs_p->exmem_preg.inp =
      stage_execute(
          pregs_p->idex_preg.out,
          pwires_p
      );

  pregs_p->memwb_preg.inp =
      stage_mem(
          pregs_p->exmem_preg.out,
          pwires_p,
          memory_p,
          cache_p
      );

  stage_writeback(
      pregs_p->memwb_preg.out,
      pwires_p,
      regfile_p
  );

  if (pwires_p->stall)
  {
    pregs_p->ifid_preg.inp =
        pregs_p->ifid_preg.out;

    pregs_p->idex_preg.inp.reg_write =
        false;

    pregs_p->idex_preg.inp.mem_read =
        false;

    pregs_p->idex_preg.inp.mem_write =
        false;

    pregs_p->idex_preg.inp.branch =
        false;

    pregs_p->idex_preg.inp.jump =
        false;

    pwires_p->stall = false;
  }

  if (pregs_p->memwb_preg.inp.branch_taken)
  {
    pwires_p->pcsrc = true;

    pwires_p->pc_src1 =
        pregs_p->memwb_preg.inp.branch_target;

    branch_counter++;

    ifid_reg_t ifid_f = {0};
    idex_reg_t idex_f = {0};
    exmem_reg_t exmem_f = {0};

    ifid_f.instr.bits = 0x00000013;
    idex_f.instr.bits = 0x00000013;
    exmem_f.instr.bits = 0x00000013;

    ifid_f.instr_addr =
        pregs_p->ifid_preg.inp.instr_addr;

    idex_f.instr_addr =
        pregs_p->idex_preg.inp.instr_addr;

    exmem_f.instr_addr =
        pregs_p->exmem_preg.inp.instr_addr;

    pregs_p->ifid_preg.inp = ifid_f;
    pregs_p->idex_preg.inp = idex_f;
    pregs_p->exmem_preg.inp = exmem_f;

#ifdef DEBUG_CYCLE
    printf("[CPL]: Pipeline Flushed\n");
#endif
  }

  pregs_p->ifid_preg.out =
      pregs_p->ifid_preg.inp;

  pregs_p->idex_preg.out =
      pregs_p->idex_preg.inp;

  pregs_p->exmem_preg.out =
      pregs_p->exmem_preg.inp;

  pregs_p->memwb_preg.out =
      pregs_p->memwb_preg.inp;

  total_cycle_counter++;

#ifdef DEBUG_REG_TRACE
  print_register_trace(regfile_p);
#endif

  if (
      pregs_p->memwb_preg.out.instr.bits ==
          0x00000073 &&
      regfile_p->R[10] == 10
  )
  {
    *ecall_exit = true;
  }
}
