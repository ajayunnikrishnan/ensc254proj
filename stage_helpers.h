#ifndef __STAGE_HELPERS_H__
#define __STAGE_HELPERS_H__

#include <stdio.h>
#include "utils.h"
#include "pipeline.h"

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

/// CORE DATAPATH HELPERS ///

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
        case ALU_SLL:  return rs1 << (rs2 & 0x1F);
        case ALU_SLT:  return (int32_t)rs1 < (int32_t)rs2 ? 1 : 0;
        case ALU_SLTU: return rs1 < rs2 ? 1 : 0;
        case ALU_XOR:  return rs1 ^ rs2;
        case ALU_SRL:  return rs1 >> (rs2 & 0x1F);
        case ALU_SRA:  return (int32_t)rs1 >> (rs2 & 0x1F);
        case ALU_OR:   return rs1 | rs2;
        case ALU_AND:  return rs1 & rs2;
        default: return 0;
    }
}

/// EXECUTE STAGE HELPERS ///

uint32_t gen_alu_control(idex_reg_t idex_reg)
{
  uint32_t alu_control = ALU_ADD;
  Instruction instr = idex_reg.instr;

  switch(instr.opcode)
  {
    case 0x33: 
      switch(instr.rtype.funct3)
      {
        case 0x0: alu_control = (instr.rtype.funct7 == 0x20) ? ALU_SUB : ALU_ADD; break;
        case 0x1: alu_control = ALU_SLL; break;
        case 0x2: alu_control = ALU_SLT; break;
        case 0x3: alu_control = ALU_SLTU; break;
        case 0x4: alu_control = ALU_XOR; break;
        case 0x5: alu_control = (instr.rtype.funct7 == 0x20) ? ALU_SRA : ALU_SRL; break;
        case 0x6: alu_control = ALU_OR; break;
        case 0x7: alu_control = ALU_AND; break;
      }
      break;
    case 0x13: 
      switch(instr.itype.funct3)
      {
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

bool gen_branch(Instruction instr, uint32_t rs1_val, uint32_t rs2_val)
{
  bool taken = false;
  switch(instr.sbtype.funct3)
  {
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


/// PIPELINE FEATURES ///

void gen_forward(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p)
{
  idex_reg_t* idex = &pregs_p->idex_preg.out;
  exmem_reg_t* exmem = &pregs_p->exmem_preg.out;
  memwb_reg_t* memwb = &pregs_p->memwb_preg.out;

  pwires_p->forwardA = 0;
  pwires_p->forwardB = 0;
  
  // grab the exact values to forward
  pwires_p->exmem_fwd_val = exmem->alu_result;
  pwires_p->memwb_fwd_val = memwb->mem_to_reg ? memwb->mem_result : memwb->alu_result;

  // ex hazard forwarding
  if (exmem->reg_write && exmem->rd != 0) {
      if (idex->use_rs1 && exmem->rd == idex->rs1) {
          pwires_p->forwardA = 1;
          fwd_exex_counter++;
      }
      if (idex->use_rs2 && exmem->rd == idex->rs2) {
          pwires_p->forwardB = 1;
          fwd_exex_counter++;
      }
  }

  // mem hazard forwarding
  if (memwb->reg_write && memwb->rd != 0) {
      // only forward from mem if ex did not already cover it
      if (idex->use_rs1 && memwb->rd == idex->rs1 &&
          !(exmem->reg_write && exmem->rd != 0 && exmem->rd == idex->rs1)) {
          pwires_p->forwardA = 2;
          fwd_exmem_counter++;
      }
      if (idex->use_rs2 && memwb->rd == idex->rs2 &&
          !(exmem->reg_write && exmem->rd != 0 && exmem->rd == idex->rs2)) {
          pwires_p->forwardB = 2;
          fwd_exmem_counter++;
      }
  }
}

void detect_hazard(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  idex_reg_t* idex = &pregs_p->idex_preg.out;
  ifid_reg_t* ifid = &pregs_p->ifid_preg.out;
  (void)regfile_p;

  pwires_p->stall = false;

  // load-use hazard detection
  if (idex->mem_read && idex->rd != 0) {
      uint32_t op = ifid->instr.opcode;
      bool uses_rs1 = false;
      bool uses_rs2 = false;
      uint32_t rs1 = 0, rs2 = 0;

      if (op == 0x33 || op == 0x63 || op == 0x23) { // r-type, branch, store
          uses_rs1 = true; uses_rs2 = true;
          rs1 = ifid->instr.rtype.rs1; rs2 = ifid->instr.rtype.rs2;
      } else if (op == 0x13 || op == 0x03 || op == 0x67) { // i-type, loads, jalr
          uses_rs1 = true;
          rs1 = ifid->instr.itype.rs1;
      }

      if ((uses_rs1 && idex->rd == rs1) || (uses_rs2 && idex->rd == rs2)) {
          pwires_p->stall = true;
          stall_counter++;
      }
  }
}

void print_register_trace(regfile_t* regfile_p)
{
  for (uint8_t i = 0; i < 8; i++)      
  {
    for (uint8_t j = 0; j < 4; j++)    
    {
      printf("r%2d=%08x ", i * 4 + j, regfile_p->R[i * 4 + j]);
    }
    printf("\n");
  }
  printf("\n");
}

#endif // __STAGE_HELPERS_H__