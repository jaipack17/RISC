#pragma once

#include <cstdint>

#include "risc/alu/alu_types.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::alu {

class Multipliers {
private:
    static Word magnitude(Word val) {
        const bool negative = (val >> (isa::WORD_BITS - 1)) != 0;
        return negative ? Word{0} - val : val;
    }

    // 1. Unsigned Shift-and-Add with Sign Negation (Slides 27-29, Solution 1)
    static Word shift_add(Word a, Word b) {
        const bool a_neg = (a >> (isa::WORD_BITS - 1)) != 0;
        const bool b_neg = (b >> (isa::WORD_BITS - 1)) != 0;
        const Word abs_a = magnitude(a);
        const Word abs_b = magnitude(b);

        Word product = 0;
        for (unsigned bit = 0; bit < isa::WORD_BITS; ++bit) {
            if (((abs_b >> bit) & 1u) != 0) {
                product += (abs_a << bit);
            }
        }

        if (a_neg ^ b_neg) {
            product = Word{0} - product;
        }
        return product;
    }

    // 2. Booth's Algorithm (Radix-2) (Slides 30-31, Solution 2)
    // Directly multiplies two's complement signed binary numbers without pre-conversion.
    // Inspects bit pairs (Q0, Q-1) and performs joint arithmetic right shifts across [A, Q, Q-1].
    static Word booth_radix2(Word a, Word b) {
        Word acc = 0;         // Accumulator A
        Word q = b;           // Multiplier Q
        unsigned q_minus_1 = 0; // Q-1 flip-flop
        const Word m = a;     // Multiplicand M

        for (unsigned step = 0; step < isa::WORD_BITS; ++step) {
            const unsigned q_0 = q & 1u;
            const unsigned pair = (q_0 << 1) | q_minus_1;

            if (pair == 0b01) {
                acc += m;
            } else if (pair == 0b10) {
                acc -= m;
            }
            // 0b00 and 0b11: do nothing (skip addition/subtraction)

            // Combined Arithmetic Right Shift: [acc, q, q_minus_1]
            q_minus_1 = q_0;
            const unsigned acc_lsb = acc & 1u;
            q = (q >> 1) | (Word(acc_lsb) << (isa::WORD_BITS - 1));
            // Arithmetic right shift on acc preserves sign bit
            const bool acc_msb = (acc >> (isa::WORD_BITS - 1)) != 0;
            acc = (acc >> 1) | (acc_msb ? (Word{1} << (isa::WORD_BITS - 1)) : 0u);
        }

        // Low 32 bits of 64-bit product reside in Q
        return q;
    }

    // 3. Modified Booth Radix-4 (Overlapping 3-bit recoding)
    // Evaluates 16 partial products with recoded factors in {-2, -1, 0, +1, +2}
    static Word booth_radix4(Word a, Word b) {
        Word product = 0;

        for (unsigned group = 0; group < isa::WORD_BITS / 2; ++group) {
            const unsigned low = (group == 0) ? 0u : ((b >> (2 * group - 1)) & 1u);
            const unsigned mid = (b >> (2 * group)) & 1u;
            const unsigned high = (b >> (2 * group + 1)) & 1u;
            const unsigned code = (high << 2) | (mid << 1) | low;

            Word partial = 0;
            bool negate = false;

            switch (code) {
                case 0b001: // +1 * M
                case 0b010:
                    partial = a;
                    break;
                case 0b011: // +2 * M
                    partial = a << 1;
                    break;
                case 0b100: // -2 * M
                    partial = a << 1;
                    negate = true;
                    break;
                case 0b101: // -1 * M
                case 0b110:
                    partial = a;
                    negate = true;
                    break;
                default: // 000, 111: 0 * M
                    break;
            }

            partial <<= (2 * group);
            product += negate ? (Word{0} - partial) : partial;
        }

        return product;
    }

public:
    static Word multiply(Word a, Word b, MultiplierAlgorithm algorithm) {
        switch (algorithm) {
            case MultiplierAlgorithm::ShiftAdd:
                return shift_add(a, b);
            case MultiplierAlgorithm::BoothRadix2:
                return booth_radix2(a, b);
            case MultiplierAlgorithm::BoothRadix4:
            default:
                return booth_radix4(a, b);
        }
    }

    static constexpr MultiplierCharacteristics characteristics(MultiplierAlgorithm algorithm) {
        switch (algorithm) {
            case MultiplierAlgorithm::ShiftAdd:
                return {"Unsigned Shift-and-Add (Magnitude conversion)", 32, 32, 12.0};
            case MultiplierAlgorithm::BoothRadix2:
                return {"Booth's Algorithm (Radix-2, Pair Checking)", 32, 32, 10.0};
            case MultiplierAlgorithm::BoothRadix4:
            default:
                return {"Modified Booth (Radix-4, 16 Partial Products)", 16, 16, 14.0};
        }
    }
};

} // namespace risc::alu
