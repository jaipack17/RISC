#pragma once

#include <cstdint>
#include <limits>

#include "risc/exception.hpp"
#include "risc/cpu/flags.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::cpu {

class ALU {
private:
    using Word = isa::Word;
    using SWord = isa::SWord;

    // Adder: add, sub, cmp and the address for ld/st
    static Word adder(Word a, Word b, const ControlSignals& ctrl, Flags& flags) {
        Word result;
        bool carry;
        bool overflow;

        if (ctrl.isSub || ctrl.isCmp) {
            result = a - b;
            carry = a >= b;
            std::int64_t exact = std::int64_t(SWord(a)) - std::int64_t(SWord(b));
            overflow = exact != std::int64_t(SWord(result));
        } else {
            result = a + b;
            carry = (std::uint64_t(a) + std::uint64_t(b)) >> isa::WORD_BITS != 0;
            std::int64_t exact = std::int64_t(SWord(a)) + std::int64_t(SWord(b));
            overflow = exact != std::int64_t(SWord(result));
        }

        if (ctrl.isCmp) {
            flags.set(isa::FLAG_E, a == b);
            flags.set(isa::FLAG_GT, SWord(a) > SWord(b));
        }

        // add, sub and cmp write C and V, but ld/st only borrow the adder for the address
        bool writes_c_v = ctrl.isSub || ctrl.isCmp || (ctrl.isAdd && !ctrl.isLd && !ctrl.isSt);
        if (writes_c_v) {
            flags.set(isa::FLAG_C, carry);
            flags.set(isa::FLAG_V, overflow);
        }
        return result;
    }

    // Multiplier: low 32 bits of the product
    static Word multiplier(Word a, Word b) {
        return a * b;
    }

    // Divider: signed, rounds toward zero, remainder takes the sign of a
    static Word divider(Word a, Word b, const ControlSignals& ctrl) {
        if (b == 0) {
            throw CpuException{isa::Exception::DivideByZero, 0};
        }

        SWord x = SWord(a);
        SWord y = SWord(b);

        // INT_MIN / -1 gives INT_MIN, remainder 0 which leads to overflow
        if (x == std::numeric_limits<SWord>::min() && y == -1) {
            if (ctrl.isDiv) {
                return a;
            }
            return 0;
        }

        if (ctrl.isDiv) {
            return Word(x / y);
        }
        return Word(x % y);
    }

    // Shift unit: only the low 5 bits of b are used as the shift amount
    static Word shift_unit(Word a, Word b, const ControlSignals& ctrl) {
        unsigned amount = b & (isa::WORD_BITS - 1);

        if (ctrl.isLsl) {
            return a << amount;
        }
        if (ctrl.isLsr) {
            return a >> amount;
        }
        return Word(SWord(a) >> amount); // asr
    }

    // Logical unit: and, or, xor, not
    static Word logical_unit(Word a, Word b, const ControlSignals& ctrl) {
        if (ctrl.isAnd) {
            return a & b;
        }
        if (ctrl.isOr) {
            return a | b;
        }
        if (ctrl.isXor) {
            return a ^ b;
        }
        return ~b;
    }

public:
    // Final multiplexer
    Word execute(Word a, Word b, const ControlSignals& ctrl, Flags& flags) const {
        if (ctrl.isAdd || ctrl.isSub || ctrl.isCmp) {
            return adder(a, b, ctrl, flags);
        }
        if (ctrl.isMul) {
            return multiplier(a, b);
        }
        if (ctrl.isDiv || ctrl.isMod) {
            return divider(a, b, ctrl);
        }
        if (ctrl.isLsl || ctrl.isLsr || ctrl.isAsr) {
            return shift_unit(a, b, ctrl);
        }
        if (ctrl.isAnd || ctrl.isOr || ctrl.isXor || ctrl.isNot) {
            return logical_unit(a, b, ctrl);
        }
        if (ctrl.isMov) {
            return b;
        }
        return 0; // no ALU operation (branches, nop, hlt)
    }
};
}
