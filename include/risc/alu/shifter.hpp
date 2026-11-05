#pragma once

#include "risc/alu/alu_types.hpp"
#include "risc/alu/gates.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::alu {

class Shifter {
public:
    // Logarithmic barrel shifter: 5 levels of muxes shift by 16, 8, 4, 2 and 1 when that bit of
    // the shift amount (the low 5 bits of b) is set. Shifted-in bits are 0, or the sign for asr.
    static Word shift(Circuit& c, Word a, Word b, ShiftOp op) {
        Bus value = to_bus(a);
        Bus amount = to_bus(b & (isa::WORD_BITS - 1), 5);
        for (int level = 4; level >= 0; level--) {
            int distance = 1 << level;
            Bit fill = constant(false);
            if (op == ShiftOp::Asr) {
                fill = value[isa::WORD_BITS - 1];
            }
            Bus next;
            for (int i = 0; i < int(isa::WORD_BITS); i++) {
                Bit moved = fill;
                if (op == ShiftOp::Lsl && i - distance >= 0) {
                    moved = value[i - distance];
                } else if (op != ShiftOp::Lsl && i + distance < int(isa::WORD_BITS)) {
                    moved = value[i + distance];
                }
                next.push_back(c.mux(amount[level], value[i], moved));
            }
            value = next;
        }
        return Word(to_number(value));
    }

    static Word shift(Word a, Word b, ShiftOp op) {
        Circuit c;
        return shift(c, a, b, op);
    }
};
}
