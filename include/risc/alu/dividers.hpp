#pragma once

#include <cstdint>
#include <limits>

#include "risc/alu/alu_types.hpp"
#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::alu {

class Dividers {
private:
    static Word magnitude(Word val) {
        const bool negative = (val >> (isa::WORD_BITS - 1)) != 0;
        return negative ? Word{0} - val : val;
    }

    // 1. Restoring Division Flowchart (Slides 33-34)
    // 32-step shift-and-subtract recurrence:
    // Left-shift [A, Q], trial subtract A = A - M, restore A = A + M if A < 0
    static DivisionResult restoring(Word dividend, Word divisor) {
        Word q = dividend;
        std::uint64_t a = 0; // 64-bit to prevent overflow during intermediate addition/subtraction
        const std::uint64_t m = divisor;

        for (unsigned step = 0; step < isa::WORD_BITS; ++step) {
            // Shift Left [A, Q] by 1
            const unsigned q_msb = (q >> (isa::WORD_BITS - 1)) & 1u;
            a = (a << 1) | q_msb;
            q <<= 1;

            // Trial subtraction: A <- A - M
            if (a >= m) {
                a -= m;
                q |= 1u; // Q0 <- 1
            } else {
                // Restore step: A < 0, keep A unchanged (implicit restore), Q0 <- 0
            }
        }

        return {q, static_cast<Word>(a)};
    }

    // 2. Non-Restoring Division (Smruti Sarangi Reference, Slide 64)
    // Avoids restoring additions by conditionally adding or subtracting based on previous remainder sign
    static DivisionResult non_restoring(Word dividend, Word divisor) {
        Word q = dividend;
        std::int64_t a = 0;
        const std::int64_t m = divisor;

        for (unsigned step = 0; step < isa::WORD_BITS; ++step) {
            const unsigned q_msb = (q >> (isa::WORD_BITS - 1)) & 1u;
            if (a >= 0) {
                a = (a << 1) | q_msb;
                a -= m;
            } else {
                a = (a << 1) | q_msb;
                a += m;
            }

            q <<= 1;
            if (a >= 0) {
                q |= 1u;
            }
        }

        // Final correction if remainder is negative
        if (a < 0) {
            a += m;
        }

        return {q, static_cast<Word>(a)};
    }

public:
    static DivisionResult divide(Word dividend, Word divisor, DividerAlgorithm algorithm) {
        if (divisor == 0) {
            throw CpuException{isa::Exception::DivideByZero, 0};
        }

        // Signed two's complement boundary overflow: INT_MIN / -1
        const SWord s_dividend = static_cast<SWord>(dividend);
        const SWord s_divisor = static_cast<SWord>(divisor);
        if (s_dividend == std::numeric_limits<SWord>::min() && s_divisor == -1) {
            return {dividend, 0};
        }

        const bool dividend_neg = (dividend >> (isa::WORD_BITS - 1)) != 0;
        const bool divisor_neg = (divisor >> (isa::WORD_BITS - 1)) != 0;
        const Word abs_dividend = magnitude(dividend);
        const Word abs_divisor = magnitude(divisor);

        DivisionResult result = (algorithm == DividerAlgorithm::Restoring)
            ? restoring(abs_dividend, abs_divisor)
            : non_restoring(abs_dividend, abs_divisor);

        // Sign correction:
        // Quotient sign is negative if operands have differing signs
        if (dividend_neg ^ divisor_neg) {
            result.quotient = Word{0} - result.quotient;
        }
        // Remainder sign always follows the dividend's sign
        if (dividend_neg) {
            result.remainder = Word{0} - result.remainder;
        }

        return result;
    }

    static constexpr DividerCharacteristics characteristics(DividerAlgorithm algorithm) {
        switch (algorithm) {
            case DividerAlgorithm::Restoring:
                return {"Restoring Division (Slide 34 Flowchart)", 32, true};
            case DividerAlgorithm::NonRestoring:
            default:
                return {"Non-Restoring Division (Sarangi Reference)", 32, false};
        }
    }
};

} // namespace risc::alu
