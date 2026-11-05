#pragma once

#include "risc/isa/isa_config.hpp"

namespace risc::four_stage_micro {

// Finds which registers an instruction reads in OF and EX, and which register it writes in MW,
// straight from its instruction bits (Sarangi §10.6)
class InterlockUnit {
private:
    static unsigned field(isa::Word ir, unsigned pos, unsigned width) {
        return (ir >> pos) & ((1u << width) - 1);
    }

    static isa::Opcode opcode(isa::Word ir) {
        return isa::Opcode(field(ir, isa::OPCODE_POS, isa::OPCODE_BITS));
    }

    // The instruction's row in the instruction table, or nullptr for an unused opcode
    static const isa::InstructionInfo* find_info(isa::Word ir) {
        for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
            if (info.opcode == opcode(ir)) {
                return &info;
            }
        }
        return nullptr;
    }

public:
    // The register the instruction writes, or -1 if it writes none
    static int written_register(isa::Word ir) {
        const isa::InstructionInfo* info = find_info(ir);
        if (info == nullptr) {
            return -1;
        }
        if (info->operands == isa::Operands::RdRs1Src2 || info->operands == isa::Operands::RdSrc2) {
            return int(field(ir, isa::RD_POS, isa::REG_BITS));
        }
        if (info->opcode == isa::Opcode::LD) {
            return int(field(ir, isa::RD_POS, isa::REG_BITS));
        }
        if (info->opcode == isa::Opcode::CALL) {
            return int(isa::REG_RA);
        }
        return -1;
    }

    // True if the instruction reads register reg in OF (its rs1)
    static bool reads_in_of(isa::Word ir, unsigned reg) {
        const isa::InstructionInfo* info = find_info(ir);
        if (info == nullptr) {
            return false;
        }
        if (info->operands == isa::Operands::RdRs1Src2 || info->operands == isa::Operands::Rs1Src2 ||
            info->operands == isa::Operands::RdMem) {
            return field(ir, isa::RS1_POS, isa::REG_BITS) == reg;
        }
        return false;
    }

    // True if the instruction reads register reg in EX (rs2, rd for st, ra for ret)
    static bool reads_in_ex(isa::Word ir, unsigned reg) {
        const isa::InstructionInfo* info = find_info(ir);
        if (info == nullptr) {
            return false;
        }
        bool has_rs2 = info->operands == isa::Operands::RdRs1Src2 || info->operands == isa::Operands::Rs1Src2 ||
                       info->operands == isa::Operands::RdSrc2;
        if (has_rs2 && field(ir, isa::I_POS, 1) == 0) {
            return field(ir, isa::RS2_POS, isa::REG_BITS) == reg;
        }
        if (info->opcode == isa::Opcode::ST) {
            return field(ir, isa::RD_POS, isa::REG_BITS) == reg;
        }
        if (info->opcode == isa::Opcode::RET) {
            return reg == isa::REG_RA;
        }
        return false;
    }
};
}
