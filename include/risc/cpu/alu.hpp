#pragma once

#include <cstdint>

#include "risc/alu/alu.hpp"
#include "risc/cpu/flags.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::cpu {

class ALU {
private:
    using Word = isa::Word;
    using SWord = isa::SWord;

    alu::OptimizedALU core_alu;

public:
    // Which adder, multiplier and divider the ALU uses
    void configure(const alu::AluConfig& config) {
        core_alu.configure(config);
    }

    // Final multiplexer
    Word execute(Word a, Word b, const ControlSignals& ctrl, Flags& flags) {
        if (ctrl.isAdd || ctrl.isSub || ctrl.isCmp) {
            alu::ALUFlags alu_flags;
            Word result = 0;
            if (ctrl.isSub || ctrl.isCmp) {
                result = core_alu.sub(a, b, alu_flags);
            } else {
                result = core_alu.add(a, b, alu_flags);
            }

            if (ctrl.isCmp) {
                flags.set(isa::FLAG_E, alu_flags.E);
                flags.set(isa::FLAG_GT, alu_flags.GT);
            }

            // add, sub and cmp write C and V, but ld/st only borrow the adder for the address
            bool writes_c_v = ctrl.isSub || ctrl.isCmp || (ctrl.isAdd && !ctrl.isLd && !ctrl.isSt);
            if (writes_c_v) {
                flags.set(isa::FLAG_C, alu_flags.C);
                flags.set(isa::FLAG_V, alu_flags.V);
            }
            return result;
        }

        if (ctrl.isMul) {
            return core_alu.mul(a, b);
        }

        if (ctrl.isDiv) {
            return core_alu.div(a, b);
        }

        if (ctrl.isMod) {
            return core_alu.mod(a, b);
        }

        if (ctrl.isLsl) {
            return core_alu.shift(a, b, alu::ShiftOp::Lsl);
        }

        if (ctrl.isLsr) {
            return core_alu.shift(a, b, alu::ShiftOp::Lsr);
        }

        if (ctrl.isAsr) {
            return core_alu.shift(a, b, alu::ShiftOp::Asr);
        }

        if (ctrl.isAnd) {
            return alu::OptimizedALU::bitwise_and(a, b);
        }

        if (ctrl.isOr) {
            return alu::OptimizedALU::bitwise_or(a, b);
        }

        if (ctrl.isXor) {
            return alu::OptimizedALU::bitwise_xor(a, b);
        }

        if (ctrl.isNot) {
            return alu::OptimizedALU::bitwise_not(b);
        }

        if (ctrl.isMov) {
            return b;
        }

        return 0; // no ALU operation (branches, nop, hlt)
    }
};

} // namespace risc::cpu
