#ifndef __STAGE_HELPERS_H__
#define __STAGE_HELPERS_H__

#include <stdio.h>
#include "utils.h"
#include "pipeline.h"

/**
 * ALU control encodings used internally by gen_alu_control() / execute_alu().
 * These correspond to the "ALU control" signal in the textbook datapath -
 * a widened version since RV32I needs more operations than MIPS' simple ALU.
 */
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

/// EXECUTE STAGE HELPERS ///

/**
 * input  : idex_reg_t
 * output : uint32_t alu_control signal
 **/
uint32_t gen_alu_control(idex_reg_t idex_reg)
{
  uint32_t alu_control = ALU_ADD;
  Instruction instr = idex_reg.instr;

  switch(instr.opcode)
  {
    case 0x33: // R-type: funct3 + funct7 select the operation
      switch(instr.rtype.funct3)
      {
        case 0x0:
          if (instr.rtype.funct7 == 0x01)      alu_control = ALU_MUL; // mul
          else if (instr.rtype.funct7 == 0x20) alu_control = ALU_SUB; // sub
          else                                  alu_control = ALU_ADD; // add
          break;
        case 0x1: alu_control = (instr.rtype.funct7 == 0x01) ? ALU_MULH : ALU_SLL; break; // mulh/sll
        case 0x2: alu_control = ALU_SLT;  break; // slt
        case 0x3: alu_control = ALU_SLTU; break; // sltu
        case 0x4: alu_control = (instr.rtype.funct7 == 0x01) ? ALU_DIV : ALU_XOR;  break; // div/xor
        case 0x5: alu_control = (instr.rtype.funct7 == 0x20) ? ALU_SRA : ALU_SRL; break; // sra/srl
        case 0x6: alu_control = (instr.rtype.funct7 == 0x01) ? ALU_REM : ALU_OR;   break; // rem/or
        case 0x7: alu_control = ALU_AND;  break; // and
        default:  alu_control = ALU_ADD;  break;
      }
      break;

    case 0x13: // I-type ALU: same funct3 encodings as R-type, funct7 comes from imm[10]
      switch(instr.itype.funct3)
      {
        case 0x0: alu_control = ALU_ADD;  break; // addi
        case 0x1: alu_control = ALU_SLL;  break; // slli
        case 0x2: alu_control = ALU_SLT;  break; // slti
        case 0x3: alu_control = ALU_SLTU; break; // sltiu
        case 0x4: alu_control = ALU_XOR;  break; // xori
        case 0x5: alu_control = ((instr.itype.imm >> 10) & 0x1) ? ALU_SRA : ALU_SRL; break; // srai/srli
        case 0x6: alu_control = ALU_OR;   break; // ori
        case 0x7: alu_control = ALU_AND;  break; // andi
        default:  alu_control = ALU_ADD;  break;
      }
      break;

    case 0x03: // Load:  address = rs1 + imm            -> add
    case 0x23: // Store: address = rs1 + imm            -> add
    case 0x37: // LUI:   result  = 0 (rs1 unused) + imm  -> add
    default:   // branches / jal / ecall: ALU result unused, default to add
      alu_control = ALU_ADD;
      break;
  }
  return alu_control;
}

/**
 * input  : alu_inp1, alu_inp2, alu_control
 * output : uint32_t alu_result
 **/
uint32_t execute_alu(uint32_t alu_inp1, uint32_t alu_inp2, uint32_t alu_control)
{
  uint32_t result;
  switch(alu_control){
    case ALU_ADD: //add
      result = alu_inp1 + alu_inp2;
      break;
    case ALU_SUB: //sub
      result = alu_inp1 - alu_inp2;
      break;
    case ALU_MUL: //mul (lower 32 bits of the product)
      result = alu_inp1 * alu_inp2;
      break;
    case ALU_AND: //and
      result = alu_inp1 & alu_inp2;
      break;
    case ALU_OR: //or
      result = alu_inp1 | alu_inp2;
      break;
    case ALU_XOR: //xor
      result = alu_inp1 ^ alu_inp2;
      break;
    case ALU_SLL: //shift left logical (only low 5 bits of shift amount matter for RV32)
      result = alu_inp1 << (alu_inp2 & 0x1F);
      break;
    case ALU_SRL: //shift right logical
      result = alu_inp1 >> (alu_inp2 & 0x1F);
      break;
    case ALU_SRA: //shift right arithmetic (sign-extending)
      result = (uint32_t)(((sWord)alu_inp1) >> (alu_inp2 & 0x1F));
      break;
    case ALU_SLT: //set less than (signed)
      result = (((sWord)alu_inp1) < ((sWord)alu_inp2)) ? 1 : 0;
      break;
    case ALU_SLTU: //set less than (unsigned)
      result = (alu_inp1 < alu_inp2) ? 1 : 0;
      break;
    case ALU_MULH: //upper 32 bits of signed 64-bit product
      result = (uint32_t)((((sDouble)(sWord)alu_inp1) * ((sDouble)(sWord)alu_inp2)) >> 32);
      break;
    case ALU_DIV: //signed division
      // divisor==0 guarded so a flushed wrong-path div can't crash the simulator
      if (alu_inp2 == 0)                    result = 0xFFFFFFFF;
      else if (alu_inp1 == 0x80000000 && alu_inp2 == 0xFFFFFFFF) result = 0x80000000; // overflow case
      else result = (uint32_t)(((sWord)alu_inp1) / ((sWord)alu_inp2));
      break;
    case ALU_REM: //signed remainder
      if (alu_inp2 == 0)                    result = alu_inp1;
      else if (alu_inp1 == 0x80000000 && alu_inp2 == 0xFFFFFFFF) result = 0;
      else result = (uint32_t)(((sWord)alu_inp1) % ((sWord)alu_inp2));
      break;
    default:
      result = 0xBADCAFFE;
      break;
  };
  return result;
}

/// DECODE STAGE HELPERS ///

/**
 * input  : Instruction
 * output : idex_reg_t
 **/
uint32_t gen_imm(Instruction instruction)
{
  int imm_val = 0;
  switch(instruction.opcode) {
        case 0x63: //B-type (branch)
            imm_val = get_branch_offset(instruction);
            break;
        case 0x13: //I-type ALU (addi, slti, xori, ...)
        case 0x03: //I-type load
            imm_val = sign_extend_number(instruction.itype.imm, 12);
            break;
        case 0x23: //S-type (store)
            imm_val = get_store_offset(instruction);
            break;
        case 0x37: //U-type (lui) - imm occupies bits [31:12], so pre-shift it
            imm_val = instruction.utype.imm << 12;
            break;
        case 0x6F: //UJ-type (jal)
            imm_val = get_jump_offset(instruction);
            break;
        default: // R-type and undefined opcodes have no immediate
            break;
    };
    return imm_val;
}

/**
 * input  : Instruction
 * output : rs1/rs2/rd numbers and whether rs1/rs2 are actually read
 *
 * note: the bit positions of rs1/rs2 alias immediate bits in other formats,
 * so the use_rs* flags matter - forwarding/stalling must not fire on
 * immediate bits that merely look like a register number.
 **/
void gen_reg_fields(Instruction instr, uint32_t* rs1, uint32_t* rs2, uint32_t* rd,
                    bool* use_rs1, bool* use_rs2)
{
  *rs1 = 0; *rs2 = 0; *rd = 0;
  *use_rs1 = false; *use_rs2 = false;
  switch(instr.opcode)
  {
    case 0x33: // r-type
      *rs1 = instr.rtype.rs1; *rs2 = instr.rtype.rs2; *rd = instr.rtype.rd;
      *use_rs1 = true; *use_rs2 = true;
      break;
    case 0x13: // i-type alu
    case 0x03: // load
      *rs1 = instr.itype.rs1; *rd = instr.itype.rd;
      *use_rs1 = true;
      break;
    case 0x23: // store
      *rs1 = instr.stype.rs1; *rs2 = instr.stype.rs2;
      *use_rs1 = true; *use_rs2 = true;
      break;
    case 0x63: // branch
      *rs1 = instr.sbtype.rs1; *rs2 = instr.sbtype.rs2;
      *use_rs1 = true; *use_rs2 = true;
      break;
    case 0x37: // lui
      *rd = instr.utype.rd;
      break;
    case 0x6F: // jal
      *rd = instr.ujtype.rd;
      break;
    default: // ecall, undefined
      break;
  }
}

/**
 * generates all the control logic that flows around in the pipeline
 * input  : Instruction
 * output : idex_reg_t
 **/
idex_reg_t gen_control(Instruction instruction)
{
  idex_reg_t idex_reg = {0};
  switch(instruction.opcode) {
      case 0x33:  //R-type: writes rd from the ALU, both operands are registers
        idex_reg.reg_write = true;
        idex_reg.alu_src   = false;
        break;
      case 0x13:  //I-type ALU: writes rd from the ALU, 2nd operand is imm
        idex_reg.reg_write = true;
        idex_reg.alu_src   = true;
        break;
      case 0x03:  //Load: address = rs1+imm, writes rd from memory
        idex_reg.reg_write  = true;
        idex_reg.alu_src    = true;
        idex_reg.mem_read   = true;
        idex_reg.mem_to_reg = true;
        break;
      case 0x23:  //Store: address = rs1+imm, writes rs2's value to memory
        idex_reg.reg_write = false;
        idex_reg.alu_src   = true;
        idex_reg.mem_write = true;
        break;
      case 0x63:  //Branch: compares rs1/rs2, may redirect PC, never writes rd
        idex_reg.reg_write = false;
        idex_reg.alu_src   = false;
        idex_reg.branch    = true;
        break;
      case 0x37:  //LUI: writes rd = imm (ALU adds 0 + imm, see stage_execute)
        idex_reg.reg_write = true;
        idex_reg.alu_src   = true;
        break;
      case 0x6F:  //JAL: writes rd = return address, unconditionally redirects PC
        idex_reg.reg_write = true;
        idex_reg.jump      = true;
        break;
      default:  // ecall and any remaining/undefined opcodes: no register write
          break;
  }
  return idex_reg;
}

/// MEMORY STAGE HELPERS ///

/**
 * evaluates whether a branch must be taken
 * input  : the branch instruction, and the two register values it compares
 * output : bool - true if the branch is taken
 *
 * Note: RV32 has six branch conditions (beq/bne/blt/bge/bltu/bgeu), so unlike
 * a simple MIPS "subtract and check zero" trick, we need the actual signed
 * and unsigned comparisons - funct3 selects which one applies.
 **/
bool gen_branch(Instruction instruction, uint32_t rs1_val, uint32_t rs2_val)
{
  bool taken = false;
  switch(instruction.sbtype.funct3)
  {
    case 0x0: taken = (rs1_val == rs2_val); break;                      // beq
    case 0x1: taken = (rs1_val != rs2_val); break;                      // bne
    case 0x4: taken = ((sWord)rs1_val <  (sWord)rs2_val); break;        // blt  (signed)
    case 0x5: taken = ((sWord)rs1_val >= (sWord)rs2_val); break;        // bge  (signed)
    case 0x6: taken = (rs1_val <  rs2_val); break;                      // bltu (unsigned)
    case 0x7: taken = (rs1_val >= rs2_val); break;                      // bgeu (unsigned)
    default:  taken = false; break;
  }
  return taken;
}


/// PIPELINE FEATURES ///

/**
 * Task   : Sets the pipeline wires for the forwarding unit's control signals
 *           based on the pipeline register values.
 * input  : pipeline_regs_t*, pipeline_wires_t*
 * output : None
*/
void gen_forward(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p)
{
  (void)pwires_p;
  idex_reg_t*  idex  = &pregs_p->idex_preg.out;   // instruction entering ex
  exmem_reg_t* exmem = &pregs_p->exmem_preg.out;  // instruction in mem
  memwb_reg_t* memwb = &pregs_p->memwb_preg.out;  // instruction in wb

  // value the wb stage is writing back this cycle
  uint32_t wb_val = memwb->mem_to_reg ? memwb->mem_result : memwb->alu_result;

  // ex hazard: exmem -> ex, takes priority over the mem hazard per operand
  bool ex_rs1  = idex->use_rs1 && idex->rs1 != 0 &&
                 exmem->reg_write && exmem->rd == idex->rs1;
  bool ex_rs2  = idex->use_rs2 && idex->rs2 != 0 &&
                 exmem->reg_write && exmem->rd == idex->rs2;
  // mem hazard: memwb -> ex, only for operands the ex hazard didn't cover
  bool mem_rs1 = !ex_rs1 && idex->use_rs1 && idex->rs1 != 0 &&
                 memwb->reg_write && memwb->rd == idex->rs1;
  bool mem_rs2 = !ex_rs2 && idex->use_rs2 && idex->rs2 != 0 &&
                 memwb->reg_write && memwb->rd == idex->rs2;

  // resolve (and print) all ex hazards before mem hazards to match the
  // reference forwarding unit
  if (ex_rs1)
  {
    idex->rs1_val = exmem->alu_result;
    fwd_exex_counter++;
    #ifdef DEBUG_CYCLE
    printf("[FWD]: Resolving EX hazard on rs1: x%d\n", idex->rs1);
    #endif
  }
  if (ex_rs2)
  {
    idex->rs2_val = exmem->alu_result;
    fwd_exex_counter++;
    #ifdef DEBUG_CYCLE
    printf("[FWD]: Resolving EX hazard on rs2: x%d\n", idex->rs2);
    #endif
  }
  if (mem_rs1)
  {
    idex->rs1_val = wb_val;
    fwd_exmem_counter++;
    #ifdef DEBUG_CYCLE
    printf("[FWD]: Resolving MEM hazard on rs1: x%d\n", idex->rs1);
    #endif
  }
  if (mem_rs2)
  {
    idex->rs2_val = wb_val;
    fwd_exmem_counter++;
    #ifdef DEBUG_CYCLE
    printf("[FWD]: Resolving MEM hazard on rs2: x%d\n", idex->rs2);
    #endif
  }
}

/**
 * Task   : Sets the pipeline wires for the hazard unit's control signals
 *           based on the pipeline register values.
 * input  : pipeline_regs_t*, pipeline_wires_t*
 * output : None
*/
void detect_hazard(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  idex_reg_t* idex = &pregs_p->idex_preg.out;   // load candidate, now in ex
  ifid_reg_t* ifid = &pregs_p->ifid_preg.out;   // use candidate, now in id

  pwires_p->stall = false;

  if (!idex->mem_read || idex->rd == 0)
    return;

  uint32_t rs1, rs2, rd;
  bool use_rs1, use_rs2;
  gen_reg_fields(ifid->instr, &rs1, &rs2, &rd, &use_rs1, &use_rs2);

  if ((use_rs1 && rs1 == idex->rd) || (use_rs2 && rs2 == idex->rd))
  {
    // load-use hazard: refetch the instruction fetched this cycle
    pwires_p->stall   = true;
    pwires_p->pc_src0 = regfile_p->PC;
    stall_counter++;
    #ifdef DEBUG_CYCLE
    printf("[HZD]: Stalling and rewriting PC: 0x%08x\n", regfile_p->PC);
    #endif
  }
}


///////////////////////////////////////////////////////////////////////////////


/// RESERVED FOR PRINTING REGISTER TRACE AFTER EACH CLOCK CYCLE ///
void print_register_trace(regfile_t* regfile_p)
{
  // print
  for (uint8_t i = 0; i < 8; i++)       // 8 columns
  {
    for (uint8_t j = 0; j < 4; j++)     // of 4 registers each
    {
      printf("r%2d=%08x ", i * 4 + j, regfile_p->R[i * 4 + j]);
    }
    printf("\n");
  }
  printf("\n");
}

#endif // __STAGE_HELPERS_H__
