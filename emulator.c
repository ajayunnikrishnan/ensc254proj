#include <stdio.h> // for stderr
#include <stdlib.h> // for exit()
#include "types.h"
#include "utils.h"
#include "riscv.h"

void execute_rtype(Instruction, Processor *);
void execute_itype_except_load(Instruction, Processor *);
void execute_branch(Instruction, Processor *);
void execute_jal(Instruction, Processor *);
void execute_load(Instruction, Processor *, Byte *);
void execute_store(Instruction, Processor *, Byte *);
void execute_ecall(Processor *, Byte *);
void execute_lui(Instruction, Processor *);

void execute_instruction(uint32_t instruction_bits, Processor *processor,Byte *memory) {    
    Instruction instruction = parse_instruction(instruction_bits);
    switch(instruction.opcode) {
        case 0x33:
            execute_rtype(instruction, processor);
            break;
        case 0x13:
            execute_itype_except_load(instruction, processor);
            break;
        case 0x73:
            execute_ecall(processor, memory);
            break;
        case 0x63:
            execute_branch(instruction, processor);
            break;
        case 0x6F:
            execute_jal(instruction, processor);
            break;
        case 0x23:
            execute_store(instruction, processor, memory);
            break;
        case 0x03:
            execute_load(instruction, processor, memory);
            break;
        case 0x37:
            execute_lui(instruction, processor);
            break;
        default: // undefined opcode
            handle_invalid_instruction(instruction);
            exit(-1);
            break;
    }
}

void execute_rtype(Instruction instruction, Processor *processor) {
    switch (instruction.rtype.funct3){
        case 0x0:
            switch (instruction.rtype.funct7) {
                case 0x0:
                    // Add
                    processor->R[instruction.rtype.rd] =
                        ((sWord)processor->R[instruction.rtype.rs1]) +
                        ((sWord)processor->R[instruction.rtype.rs2]);
                    break;
                case 0x1:
                    // Mul
                    processor->R[instruction.rtype.rd] =
                        ((sWord)processor->R[instruction.rtype.rs1]) *
                        ((sWord)processor->R[instruction.rtype.rs2]);
                    break;
                case 0x20:
                    // Sub
                    processor->R[instruction.rtype.rd] =
                        ((sWord)processor->R[instruction.rtype.rs1]) -
                        ((sWord)processor->R[instruction.rtype.rs2]);
                    break;
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
            break;
        /* YOUR CODE HERE */
        /* deal with other cases */
        case 0x1: 
            switch (instruction.rtype.funct7) {

                case 0x0:
                    // SLL shift left logical
                    processor->R[instruction.rtype.rd] =
                        processor->R[instruction.rtype.rs1] <<
                        (processor->R[instruction.rtype.rs2] & 0x1F);
                    break;
                case 0x1:
                    // MULH upper 32 bits of signed * signed (64-bit product)
                    {
                        sDouble product = (sDouble)(sWord)processor->R[instruction.rtype.rs1] * (sDouble)(sWord)processor->R[instruction.rtype.rs2];
                            processor->R[instruction.rtype.rd] = (Word)(product >> 32);
                    }
                    break;   
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
            break;
        case 0x2: 
            switch (instruction.rtype.funct7) {

                case 0x0:
                    //slt: set les than (signed)
                    processor->R[instruction.rtype.rd] =
                        ((sWord)processor->R[instruction.rtype.rs1] <
                         (sWord)processor->R[instruction.rtype.rs2]) ? 1: 0;
                    break;
                
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
            break;
        
        case 0x4:
            switch (instruction.rtype.funct7) {

                case 0x0:
                    //xor
                    processor->R[instruction.rtype.rd] = 
                        processor->R[instruction.rtype.rs1] ^
                        processor->R[instruction.rtype.rs2];
                    break;
                
                case 0x1:
                    //div: signed integer division
                    processor->R[instruction.rtype.rd] = 
                        (sWord)processor->R[instruction.rtype.rs1] / 
                        (sWord)processor->R[instruction.rtype.rs2];
                    break;
                
                default: 
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
            break;

        case 0x5:
            switch (instruction.rtype.funct7) {

                case 0x0:
                //srl shift right logical (unsigned)
                    processor->R[instruction.rtype.rd] = 
                        processor->R[instruction.rtype.rs1] >>
                        (processor->R[instruction.rtype.rs2] & 0x1F);
                    break;

                case 0x20:
                //sra shift right arithmetic (signed)
                    processor->R[instruction.rtype.rd] = 
                        (sWord)processor->R[instruction.rtype.rs1] >>
                        (processor->R[instruction.rtype.rs2] & 0x1F);
                    break;
                
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
            break;
        
        case 0x6:
            switch (instruction.rtype.funct7) {

                case 0x0:
                // or
                    processor->R[instruction.rtype.rd] = 
                        processor->R[instruction.rtype.rs1] |
                        processor->R[instruction.rtype.rs2];
                    break;
                
                case 0x1:
                //rem signed remainder
                    processor->R[instruction.rtype.rd] = 
                        (sWord)processor->R[instruction.rtype.rs1] %
                        (sWord)processor->R[instruction.rtype.rs2];
                    break;
                
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
            break;

        case 0x3:
            switch(instruction.rtype.funct7) {

                case 0x0:
                    //sltu
                    processor->R[instruction.rtype.rd] = 
                        (processor->R[instruction.rtype.rs1] < processor->R[instruction.rtype.rs2]) ? 1 : 0;
                    break;

                case 0x1: 
                    //mulhu upper 32 bits of unsinged*unsigned 64 bit profuct
                    {
                        Double product = (Double)processor->R[instruction.rtype.rs1] * (Double)processor->R[instruction.rtype.rs2];
                        processor->R[instruction.rtype.rd] = (Word)(product >> 32);
                    }
                    break;
                
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;                
            }
            break;  // was missing: sltu/mulhu fell through into the f3=7 (and) case

        case 0x7: 
            switch (instruction.rtype.funct7) {

                case 0x0:
                //and
                    processor->R[instruction.rtype.rd] = 
                        processor->R[instruction.rtype.rs1] &
                        processor->R[instruction.rtype.rs2];
                    break;
                
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
            break;

        default:
            handle_invalid_instruction(instruction);
            exit(-1);
            break;
    }
    // update PC
    processor->PC += 4;
}

void execute_itype_except_load(Instruction instruction, Processor *processor) {
    //add a sign extension so would compute 0 + (-1) = -1 instead of 0 + 4095
    int imm = sign_extend_number(instruction.itype.imm, 12);
    
    switch (instruction.itype.funct3) {
        /* YOUR CODE HERE */
        case 0x0:
            //addi
            processor->R[instruction.itype.rd] = 
                (sWord)processor->R[instruction.itype.rs1] + imm;
            break;
        
        case 0x1:
            //slli: shift left logical by immediate (shamt = lower 5 bits of imm)
            processor->R[instruction.itype.rd] = 
                processor->R[instruction.itype.rs1] << (instruction.itype.imm & 0x1F);
            break;
        
        case 0x2:
            //slti: set less than immediate (signed)
            processor->R[instruction.itype.rd] = 
                ((sWord)processor->R[instruction.itype.rs1] < imm) ? 1 : 0;
            break;

        case 0x4:
            //xori
            processor->R[instruction.itype.rd] = 
                processor->R[instruction.itype.rs1] ^ (Word)imm;
            break;
        
        case 0x5:
            // srli else srai distinguished by bit 10 of imm
            if ((instruction.itype.imm >> 10) & 0x01) {
                //srai shift right arithmetic immediate
                processor->R[instruction.itype.rd] = 
                    (sWord)processor->R[instruction.itype.rs1] >> 
                    (instruction.itype.imm & 0x1F);
            } else {
                //srli shift right logical immediate
                processor->R[instruction.itype.rd] = 
                    processor->R[instruction.itype.rs1] >>
                    (instruction.itype.imm & 0x01F);
            }
            break;

        case 0x6:
            //ori
            processor->R[instruction.itype.rd] = 
                processor->R[instruction.itype.rs1] | (Word)imm;
            break;

        case 0x7:
            //andi
            processor->R[instruction.itype.rd] = 
                processor->R[instruction.itype.rs1] & (Word)imm;
            break;

        default:
            handle_invalid_instruction(instruction);
            break;
    }
    processor->PC += 4; //prevents infinite loop and tells program to next instruction. 
}

void execute_ecall(Processor *p, Byte *memory) {
    Register i;
    
    // syscall number is given by a0 (x10)
    // argument is given by a1
    switch(p->R[10]) {
        case 1: // print an integer
            printf("%d",p->R[11]);
            p->PC += 4;
            break;
        case 4: // print a string
            for(i=p->R[11];i<MEMORY_SPACE && load(memory,i,LENGTH_BYTE);i++) {
                printf("%c",load(memory,i,LENGTH_BYTE));
            }
            p->PC += 4;
            break;
        case 10: // exit
            printf("exiting the simulator\n");
            exit(0);
            break;
        case 11: // print a character
            printf("%c",p->R[11]);
            p->PC += 4;
            break;
        default: // undefined ecall
            printf("Illegal ecall number %d\n", p->R[10]);
            exit(-1);
            break;
    }
}

void execute_branch(Instruction instruction, Processor *processor) {
    
    int offset = get_branch_offset(instruction);
    int taken = 0;

    switch (instruction.sbtype.funct3) {
        /* YOUR CODE HERE */
        case 0x0:
            //beq 
            taken = ((sWord)processor->R[instruction.sbtype.rs1] == 
                    (sWord)processor->R[instruction.sbtype.rs2]);
            break;
        
        case 0x1:
            //bne
            taken = ((sWord)processor->R[instruction.sbtype.rs1] !=
                    (sWord)processor->R[instruction.sbtype.rs2]);
            break;
        
        case 0x4: 
            //blt
            taken = ((sWord)processor->R[instruction.sbtype.rs1] <
                    (sWord)processor->R[instruction.sbtype.rs2]);
            break;

        case 0x5:
            //bge signed 
            taken = ((sWord)processor->R[instruction.sbtype.rs1] >=
                    (sWord)processor->R[instruction.sbtype.rs2]);
            break;

        case 0x6:
            //bltu unsinged less than
            taken = (processor->R[instruction.sbtype.rs1] < 
                    processor->R[instruction.sbtype.rs2]);
            break;

        case 0x7:
            //bgeu unsingde greater or equal
            taken = (processor->R[instruction.sbtype.rs1] >=
                    processor->R[instruction.sbtype.rs2]);
            break;

        default:
            handle_invalid_instruction(instruction);
            exit(-1);
            break;
    }
    if (taken)
        processor->PC +=offset;
    else    
        processor->PC += 4;
}

void execute_load(Instruction instruction, Processor *processor, Byte *memory) {

    int imm = sign_extend_number(instruction.itype.imm, 12);
    Address addr = (Address)((sWord)processor->R[instruction.itype.rs1] + imm);

    switch (instruction.itype.funct3) {
        /* YOUR CODE HERE */
        case 0x0:
            // lb: load byte, sign-extended
            processor->R[instruction.itype.rd] =
                sign_extend_number(load(memory, addr, LENGTH_BYTE), 8);
            break;
        case 0x1:
            // lh: load half-word, sign-extended
            processor->R[instruction.itype.rd] =
                sign_extend_number(load(memory, addr, LENGTH_HALF_WORD), 16);
            break;
        case 0x2:
            // lw: load word
            processor->R[instruction.itype.rd] =
                load(memory, addr, LENGTH_WORD);
            break;
       
        default:
            handle_invalid_instruction(instruction);
            break;
    }
    processor->PC += 4;
}

void execute_store(Instruction instruction, Processor *processor, Byte *memory) {

    int offset = get_store_offset(instruction);
    Address addr = (Address)((sWord)processor->R[instruction.stype.rs1] + offset);

    switch (instruction.stype.funct3) {
        /* YOUR CODE HERE */
        case 0x0:
            //sb
            store(memory, addr, LENGTH_BYTE, processor->R[instruction.stype.rs2]);
            break;
        case 0x1:
            //sh
            store(memory, addr, LENGTH_HALF_WORD, processor->R[instruction.stype.rs2]);
            break;
        case 0x2:
            //sw
            store(memory , addr, LENGTH_WORD, processor->R[instruction.stype.rs2]);
            break;

        default:
            handle_invalid_instruction(instruction);
            exit(-1);
            break;
    }
    processor->PC += 4;
}

void execute_jal(Instruction instruction, Processor *processor) {
    /* YOUR CODE HERE */
    // JAL : rd = PC+4, PC = PC += offset
    int offset = get_jump_offset(instruction);
    processor->R[instruction.ujtype.rd] = processor->PC + 4;
    processor->PC += offset;
}

void execute_lui(Instruction instruction, Processor *processor) {
    /* YOUR CODE HERE */
    //LUI: rd = imm << 12 --upper 20 bits are stored into imm field already
    processor->R[instruction.utype.rd] = instruction.utype.imm << 12;
    processor->PC += 4;
}

void store(Byte *memory, Address address, Alignment alignment, Word value) {
    /* YOUR CODE HERE */
    if (alignment == LENGTH_BYTE) {
        memory[address] = value & 0xFF;
    } else if (alignment == LENGTH_HALF_WORD) {
        memory[address] = value & 0xFF;
        memory[address + 1] = (value >> 8) & 0xFF;
    } else if (alignment == LENGTH_WORD) {
        memory[address] = value & 0xFF;
        memory[address + 1] = (value >> 8) & 0xFF;
        memory[address + 2] = (value >> 16) & 0xFF;
        memory[address + 3] = (value >> 24) & 0xFF;
    } else {
        printf("Error: Unrecognized alignment %d\n", alignment);
        exit(-1);
    }
}

Word load(Byte *memory, Address address, Alignment alignment) {
    if(alignment == LENGTH_BYTE) {
        return memory[address];
    } else if(alignment == LENGTH_HALF_WORD) {
        return (memory[address+1] << 8) + memory[address];
    } else if(alignment == LENGTH_WORD) {
        return (memory[address+3] << 24) + (memory[address+2] << 16)
               + (memory[address+1] << 8) + memory[address];
    } else {
        printf("Error: Unrecognized alignment %d\n", alignment);
        exit(-1);
    }
}
