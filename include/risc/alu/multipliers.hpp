#pragma once

#include "risc/alu/adders.hpp"
#include "risc/alu/alu_types.hpp"
#include "risc/alu/gates.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::alu {

// Sequential multipliers: one step per clock, each step at most one pass through the adder.
// They give the low 32 bits of the product. The multiplicand is shifted left one place per bit
// of the multiplier, and the product register adds it in (the low 32 bits of the textbook's
// [A, Q] register pair).
class Multipliers {
private:
    static bool negative(Word value) {
        return (value >> (isa::WORD_BITS - 1)) != 0;
    }

    // One step: 0 - value
    static Word negate(Circuit& c, Word value, AdderAlgorithm adder) {
        AdderOutput out = Adders::add_or_subtract(c, to_bus(0), to_bus(value), constant(true), adder);
        c.clock();
        return Word(to_number(out.sum));
    }

    // Slides 27-29: multiply the magnitudes, then negate the product if the signs differ
    static Word shift_add(Circuit& c, Word a, Word b, AdderAlgorithm adder) {
        Word m = a;
        if (negative(a)) {
            m = negate(c, a, adder);
        }
        Word q = b;
        if (negative(b)) {
            q = negate(c, b, adder);
        }
        Word product = 0;
        for (unsigned bit = 0; bit < isa::WORD_BITS; bit++) {
            if (((q >> bit) & 1u) != 0) {
                AdderOutput out = Adders::add_or_subtract(c, to_bus(product), to_bus(m << bit), constant(false), adder);
                product = Word(to_number(out.sum));
            }
            c.clock(); // a step that adds nothing only shifts
        }
        if (negative(a) != negative(b)) {
            product = negate(c, product, adder);
        }
        return product;
    }

    // Slides 30-31: bit pair (Q0, Q-1) = 10 subtracts M, 01 adds M, 00 and 11 only shift.
    // Works on two's complement directly.
    static Word booth_radix2(Circuit& c, Word a, Word b, AdderAlgorithm adder) {
        Word product = 0;
        unsigned previous = 0; // Q-1
        for (unsigned bit = 0; bit < isa::WORD_BITS; bit++) {
            unsigned current = (b >> bit) & 1u;
            if (current != previous) {
                bool subtract = current == 1;
                AdderOutput out = Adders::add_or_subtract(c, to_bus(product), to_bus(a << bit), constant(subtract), adder);
                product = Word(to_number(out.sum));
            }
            c.clock();
            previous = current;
        }
        return product;
    }

    // Radix-4 Booth: bits (2i+1, 2i, 2i-1) of the multiplier pick 0, ±M or ±2M; a mux picks
    // M or 2M, and the adder adds or subtracts it. 16 steps.
    static Word booth_radix4(Circuit& c, Word a, Word b, AdderAlgorithm adder) {
        Word product = 0;
        for (unsigned group = 0; group < isa::WORD_BITS / 2; group++) {
            unsigned low = 0;
            if (group > 0) {
                low = (b >> (2 * group - 1)) & 1u;
            }
            unsigned middle = (b >> (2 * group)) & 1u;
            unsigned high = (b >> (2 * group + 1)) & 1u;
            unsigned code = (high << 2) | (middle << 1) | low;

            bool add_anything = code != 0b000 && code != 0b111;
            bool twice = code == 0b011 || code == 0b100;
            bool subtract = high == 1;
            if (add_anything) {
                Bus once_m = to_bus(a << (2 * group));
                Bus twice_m = to_bus(a << (2 * group + 1));
                Bus chosen;
                for (unsigned i = 0; i < isa::WORD_BITS; i++) {
                    chosen.push_back(c.mux(constant(twice), once_m[i], twice_m[i]));
                }
                AdderOutput out = Adders::add_or_subtract(c, to_bus(product), chosen, constant(subtract), adder);
                product = Word(to_number(out.sum));
            }
            c.clock();
        }
        return product;
    }

public:
    static Word multiply(Circuit& c, Word a, Word b, MultiplierAlgorithm algorithm, AdderAlgorithm adder) {
        if (algorithm == MultiplierAlgorithm::ShiftAdd) {
            return shift_add(c, a, b, adder);
        }
        if (algorithm == MultiplierAlgorithm::BoothRadix2) {
            return booth_radix2(c, a, b, adder);
        }
        return booth_radix4(c, a, b, adder);
    }

    static Word multiply(Word a, Word b, MultiplierAlgorithm algorithm) {
        Circuit c;
        return multiply(c, a, b, algorithm, AluConfig{}.adder);
    }

    static const char* name(MultiplierAlgorithm algorithm) {
        switch (algorithm) {
            case MultiplierAlgorithm::ShiftAdd:
                return "Shift-and-add (magnitudes)";
            case MultiplierAlgorithm::BoothRadix2:
                return "Booth radix-2";
            case MultiplierAlgorithm::BoothRadix4:
                return "Booth radix-4";
        }
        return "";
    }
};
}
