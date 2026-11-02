#pragma once

#include <cstdint>

#include "risc/alu/alu_types.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::alu {

class Shifter {
public:
    // 5-stage Logarithmic Barrel Shifter (Stages 16 -> 8 -> 4 -> 2 -> 1)
    static Word shift(Word a, Word b, ShiftOp op) {
        const unsigned amount = b & (isa::WORD_BITS - 1); // low 5 bits (0 to 31)
        Word result = a;

        for (unsigned distance = isa::WORD_BITS >> 1; distance > 0; distance >>= 1) {
            if ((amount & distance) == 0) {
                continue;
            }

            if (op == ShiftOp::Lsl) {
                result <<= distance;
            } else if (op == ShiftOp::Lsr) {
                result >>= distance;
            } else { // ShiftOp::Asr
                const bool negative = (result >> (isa::WORD_BITS - 1)) != 0;
                result = static_cast<Word>(static_cast<SWord>(result) >> distance);
                if (negative) {
                    result |= (~Word{0}) << (isa::WORD_BITS - distance);
                }
            }
        }

        return result;
    }
};

} // namespace risc::alu
