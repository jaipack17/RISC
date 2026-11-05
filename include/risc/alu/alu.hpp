#pragma once

#include <cstdint>

#include "risc/alu/adders.hpp"
#include "risc/alu/alu_types.hpp"
#include "risc/alu/dividers.hpp"
#include "risc/alu/multipliers.hpp"
#include "risc/alu/shifter.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::alu {

struct ALUFlags {
    bool E = false;  // Equal (Zero)
    bool GT = false; // Signed Greater Than: (N == V) && !E
    bool C = false;  // Carry
    bool V = false;  // Signed Overflow
};

class OptimizedALU {
private:
    AdderAlgorithm adder_algorithm;
    MultiplierAlgorithm multiplier_algorithm;
    DividerAlgorithm divider_algorithm;

public:
    explicit OptimizedALU(
        AdderAlgorithm adder = AdderAlgorithm::CarryLookahead,
        MultiplierAlgorithm multiplier = MultiplierAlgorithm::BoothRadix2,
        DividerAlgorithm divider = DividerAlgorithm::Restoring)
        : adder_algorithm(adder), multiplier_algorithm(multiplier),
          divider_algorithm(divider) {}

    // Algorithm selection
    void select_adder(AdderAlgorithm algo) { adder_algorithm = algo; }
    void select_multiplier(MultiplierAlgorithm algo) { multiplier_algorithm = algo; }
    void select_divider(DividerAlgorithm algo) { divider_algorithm = algo; }

    AdderAlgorithm selected_adder() const { return adder_algorithm; }
    MultiplierAlgorithm selected_multiplier() const { return multiplier_algorithm; }
    DividerAlgorithm selected_divider() const { return divider_algorithm; }

    // Characteristics reporting
    AdderCharacteristics adder_characteristics() const {
        return Adders::characteristics(adder_algorithm);
    }
    MultiplierCharacteristics multiplier_characteristics() const {
        return Multipliers::characteristics(multiplier_algorithm);
    }
    DividerCharacteristics divider_characteristics() const {
        return Dividers::characteristics(divider_algorithm);
    }

    // Core addition / subtraction / comparison with two's complement flag logic
    Word add(Word a, Word b, ALUFlags& flags) const {
        const AdderResult res = Adders::add(a, b, false, adder_algorithm);
        flags.E = (res.sum == 0);
        flags.C = res.carry_out;
        flags.V = res.overflow;
        const bool negative = (res.sum >> (isa::WORD_BITS - 1)) != 0;
        flags.GT = (!flags.E) && (negative == flags.V);
        return res.sum;
    }

    Word sub(Word a, Word b, ALUFlags& flags) const {
        // a - b = a + (~b) + 1 (Slide 6)
        const AdderResult res = Adders::add(a, ~b, true, adder_algorithm);
        flags.E = (res.sum == 0);
        flags.C = res.carry_out; // In x86/ARM/RISC, carry_out=1 indicates no borrow (a >= b unsigned)
        flags.V = res.overflow;
        const bool negative = (res.sum >> (isa::WORD_BITS - 1)) != 0;
        // Rigorous signed greater-than: GT = (N == V) && !E
        flags.GT = (!flags.E) && (negative == flags.V);
        return res.sum;
    }

    void cmp(Word a, Word b, ALUFlags& flags) const {
        sub(a, b, flags);
    }

    Word mul(Word a, Word b) const {
        return Multipliers::multiply(a, b, multiplier_algorithm);
    }

    Word div(Word a, Word b) const {
        return Dividers::divide(a, b, divider_algorithm).quotient;
    }

    Word mod(Word a, Word b) const {
        return Dividers::divide(a, b, divider_algorithm).remainder;
    }

    Word shift(Word a, Word b, ShiftOp op) const {
        return Shifter::shift(a, b, op);
    }

    static Word bitwise_and(Word a, Word b) { return a & b; }
    static Word bitwise_or(Word a, Word b)  { return a | b; }
    static Word bitwise_xor(Word a, Word b) { return a ^ b; }
    static Word bitwise_not(Word b)         { return ~b; }
};

} // namespace risc::alu
