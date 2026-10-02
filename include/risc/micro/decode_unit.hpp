#pragma once

#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::micro {

class DecodeUnit {
private:
    using Word = isa::Word;

    static Word extract_bits(Word ir, unsigned pos, unsigned width) {
        Word mask = (Word{1} << width) - 1;
        return (ir >> pos) & mask;
    }

    static Word sign_extend(Word value, unsigned width) {
        unsigned shift = isa::WORD_BITS - width;
        return Word(isa::SWord(value << shift) >> shift);
    }

    static const isa::InstructionInfo* find_info(Word ir) {
        isa::Opcode opcode = isa::Opcode(extract_bits(ir, isa::OPCODE_POS, isa::OPCODE_BITS));
        for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
            if (info.opcode == opcode) {
                return &info;
            }
        }
        return nullptr;
    }

    // Check validity of the instruction encoding, and raise IllegalInstruction exception where invalid.
    static void check_legal(Word pc, Word ir) {
        const isa::InstructionInfo* info = find_info(ir);
        if (info == nullptr) {
            throw CpuException{isa::Exception::IllegalInstruction, pc};
        }

        bool has_i_bit = info->operands != isa::Operands::Label && info->operands != isa::Operands::None;
        bool i_bit = extract_bits(ir, isa::I_POS, 1) == 1;
        Word mod = extract_bits(ir, isa::MOD_POS, isa::MOD_BITS);

        if (has_i_bit && i_bit && mod == 0b11) {
            throw CpuException{isa::Exception::IllegalInstruction, pc};
        }
        if (info->operands == isa::Operands::RdMem) {
            if (!i_bit || mod != Word(isa::Modifier::Default)) {
                throw CpuException{isa::Exception::IllegalInstruction, pc};
            }
        }
    }

    static Word calculate_immediate(Word ir) {
        Word imm = extract_bits(ir, isa::IMM_POS, isa::IMM_BITS);
        Word mod = extract_bits(ir, isa::MOD_POS, isa::MOD_BITS);

        if (mod == Word(isa::Modifier::Unsigned)) {
            return imm;
        }
        if (mod == Word(isa::Modifier::High)) {
            return imm << isa::IMM_BITS;
        }
        return sign_extend(imm, isa::IMM_BITS);
    }

public:
    bool I = false;
    unsigned rd = 0;
    unsigned rs1 = 0;
    unsigned rs2 = 0;
    Word immx = 0;
    Word branchTarget = 0;

    void reset() {
        I = false;
        rd = 0;
        rs1 = 0;
        rs2 = 0;
        immx = 0;
        branchTarget = 0;
    }

    void decode(Word ir, Word pc) {
        check_legal(pc, ir);

        I = extract_bits(ir, isa::I_POS, 1) == 1;
        rd = extract_bits(ir, isa::RD_POS, isa::REG_BITS);
        rs1 = extract_bits(ir, isa::RS1_POS, isa::REG_BITS);
        rs2 = extract_bits(ir, isa::RS2_POS, isa::REG_BITS);
        immx = calculate_immediate(ir);

        Word offset = sign_extend(extract_bits(ir, isa::OFFSET_POS, isa::OFFSET_BITS), isa::OFFSET_BITS);
        branchTarget = pc + (offset << 2);
    }
};
}
