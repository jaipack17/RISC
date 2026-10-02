#pragma once

#include "risc/exception.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/cpu/register_file.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::cpu {

struct OperandFetchOutput {
    isa::Word op1 = 0;
    isa::Word op2 = 0;
    isa::Word immx = 0;
    isa::Word branchTarget = 0;
};

class OperandFetchUnit {
private:
    using Word = isa::Word;

    static Word extract_bits(Word inst, unsigned pos, unsigned width) {
        Word mask = (Word{1} << width) - 1;
        return (inst >> pos) & mask;
    }

    static Word sign_extend(Word value, unsigned width) {
        Word sign_bit = Word{1} << (width - 1);
        if (value & sign_bit) {
            Word mask = (Word{1} << width) - 1;
            value = value | ~mask; // fill the upper bits with 1s
        }
        return value;
    }

    static const isa::InstructionInfo* find_info(isa::Opcode opcode) {
        for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
            if (info.opcode == opcode) {
                return &info;
            }
        }
        return nullptr;
    }

    // Throws IllegalInstruction for invalid instructions
    static void check_legal(Word pc, Word inst) {
        const isa::InstructionInfo* info = find_info(opcode(inst));
        if (info == nullptr) {
            throw CpuException{isa::Exception::IllegalInstruction, pc};
        }

        bool has_i_bit = info->operands != isa::Operands::Label && info->operands != isa::Operands::None;
        Word modifier = extract_bits(inst, isa::MOD_POS, isa::MOD_BITS);

        if (has_i_bit && i_bit(inst) && modifier == 0b11) {
            throw CpuException{isa::Exception::IllegalInstruction, pc};
        }

        if (info->operands == isa::Operands::RdMem) {
            if (!i_bit(inst) || modifier != Word(isa::Modifier::Default)) {
                throw CpuException{isa::Exception::IllegalInstruction, pc};
            }
        }
    }

    // calculate immediate
    static Word calculate_immediate(Word inst) {
        Word imm = extract_bits(inst, isa::IMM_POS, isa::IMM_BITS);
        Word modifier = extract_bits(inst, isa::MOD_POS, isa::MOD_BITS);
        Word immx = imm;

        if (modifier == Word(isa::Modifier::Unsigned)) {
            immx = imm;
        }
        if (modifier == Word(isa::Modifier::High)) {
            immx = imm << isa::IMM_BITS;
        }
        if (modifier == Word(isa::Modifier::Default)) {
            immx = sign_extend(imm, isa::IMM_BITS);
        }
        return immx;
    }

    // shift by 2 bits and extend sign, then add pc
    static Word calculate_branch_target(Word pc, Word inst) {
        Word offset = sign_extend(extract_bits(inst, isa::OFFSET_POS, isa::OFFSET_BITS), isa::OFFSET_BITS);
        Word branchTarget = pc + (offset << 2);

        return branchTarget;
    }

public:
    // The two inputs of the control unit
    static isa::Opcode opcode(Word inst) {
        return isa::Opcode(extract_bits(inst, isa::OPCODE_POS, isa::OPCODE_BITS));
    }

    static bool i_bit(Word inst) {
        return extract_bits(inst, isa::I_POS, 1) == 1;
    }

    // Word = uint32_t

    OperandFetchOutput run(Word pc, Word inst, const ControlSignals& ctrl, RegisterFile& rf) const {
        check_legal(pc, inst);

        unsigned rd = extract_bits(inst, isa::RD_POS, isa::REG_BITS);
        unsigned rs1 = extract_bits(inst, isa::RS1_POS, isa::REG_BITS);
        unsigned rs2 = extract_bits(inst, isa::RS2_POS, isa::REG_BITS);

        // Multiplexer in front of read port 1: ra for ret, else rs1
        if (ctrl.isRet) {
            rf.read_port1.address = isa::REG_RA;
        } else {
            rf.read_port1.address = rs1;
        }

        // Multiplexer in front of read port 2: rd for st (the value to store), else rs2
        if (ctrl.isSt) {
            rf.read_port2.address = rd;
        } else {
            rf.read_port2.address = rs2;
        }

        rf.read();

        OperandFetchOutput out;
        out.op1 = rf.read_port1.data;
        out.op2 = rf.read_port2.data;
        out.immx = calculate_immediate(inst);
        out.branchTarget = calculate_branch_target(pc, inst);
        return out;
    }
};
}
