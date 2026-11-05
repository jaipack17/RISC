#pragma once

#include <cstdint>

#include "risc/alu/adders.hpp"
#include "risc/alu/alu_types.hpp"
#include "risc/alu/gates.hpp"
#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::alu {

// Sequential dividers on the magnitudes, one adder pass per step, 32 steps for the 32 quotient
// bits. The signs are fixed after: the quotient is negative when the signs differ, and the
// remainder takes the sign of the dividend (rounds toward zero, like C++).
class Dividers {
private:
    static constexpr unsigned WIDE = isa::WORD_BITS + 1; // the non-restoring remainder needs a sign bit

    static bool negative(Word value) {
        return (value >> (isa::WORD_BITS - 1)) != 0;
    }

    // One step: 0 - value
    static Word negate(Circuit& c, Word value, AdderAlgorithm adder) {
        AdderOutput out = Adders::add_or_subtract(c, to_bus(0), to_bus(value), constant(true), adder);
        c.clock();
        return Word(to_number(out.sum));
    }

    // Slides 33-34: shift [A, Q] left, A = A - M; if A went negative, add M back in another step
    // (the restore) and Q0 = 0, else Q0 = 1. A < M always, so 2A + 1 fits in 32 bits, and
    // A - M is negative exactly when the adder has no carry out.
    static DivisionResult restoring(Circuit& c, Word dividend, Word divisor, AdderAlgorithm adder) {
        Word a = 0;
        Word q = dividend;
        for (unsigned step = 0; step < isa::WORD_BITS; step++) {
            a = (a << 1) | (q >> (isa::WORD_BITS - 1));
            q <<= 1;
            AdderOutput trial = Adders::add_or_subtract(c, to_bus(a), to_bus(divisor), constant(true), adder);
            c.clock();
            Word difference = Word(to_number(trial.sum));
            if (trial.carry_out.value) {
                a = difference;
                q |= 1u;
            } else {
                AdderOutput restored = Adders::add_or_subtract(c, to_bus(difference), to_bus(divisor), constant(false), adder);
                c.clock();
                a = Word(to_number(restored.sum));
            }
        }
        return {q, a};
    }

    // Shift [A, Q] left; subtract M if A is not negative, else add M; Q0 = 1 if A is now not
    // negative. A is 33 bits wide (it goes down to -2M). One correction step at the end if A < 0.
    static DivisionResult non_restoring(Circuit& c, Word dividend, Word divisor, AdderAlgorithm adder) {
        const std::uint64_t mask = (std::uint64_t{1} << WIDE) - 1;
        std::uint64_t a = 0;
        Word q = dividend;
        for (unsigned step = 0; step < isa::WORD_BITS; step++) {
            bool a_negative = ((a >> isa::WORD_BITS) & 1u) != 0;
            a = ((a << 1) | (q >> (isa::WORD_BITS - 1))) & mask;
            q <<= 1;
            AdderOutput out = Adders::add_or_subtract(c, to_bus(a, WIDE), to_bus(divisor, WIDE), constant(!a_negative), adder);
            c.clock();
            a = to_number(out.sum);
            if (((a >> isa::WORD_BITS) & 1u) == 0) {
                q |= 1u;
            }
        }
        if (((a >> isa::WORD_BITS) & 1u) != 0) {
            AdderOutput out = Adders::add_or_subtract(c, to_bus(a, WIDE), to_bus(divisor, WIDE), constant(false), adder);
            c.clock();
            a = to_number(out.sum);
        }
        return {q, Word(a)};
    }

public:
    static DivisionResult divide(Circuit& c, Word dividend, Word divisor, DividerAlgorithm algorithm, AdderAlgorithm adder) {
        if (divisor == 0) {
            throw CpuException{isa::Exception::DivideByZero, 0};
        }
        // The magnitude of INT_MIN is 2^31 as an unsigned number, so INT_MIN / -1 = INT_MIN, remainder 0
        Word abs_dividend = dividend;
        if (negative(dividend)) {
            abs_dividend = negate(c, dividend, adder);
        }
        Word abs_divisor = divisor;
        if (negative(divisor)) {
            abs_divisor = negate(c, divisor, adder);
        }

        DivisionResult result;
        if (algorithm == DividerAlgorithm::Restoring) {
            result = restoring(c, abs_dividend, abs_divisor, adder);
        } else {
            result = non_restoring(c, abs_dividend, abs_divisor, adder);
        }

        if (negative(dividend) != negative(divisor)) {
            result.quotient = negate(c, result.quotient, adder);
        }
        if (negative(dividend)) {
            result.remainder = negate(c, result.remainder, adder);
        }
        return result;
    }

    static DivisionResult divide(Word dividend, Word divisor, DividerAlgorithm algorithm) {
        Circuit c;
        return divide(c, dividend, divisor, algorithm, AluConfig{}.adder);
    }

    static const char* name(DividerAlgorithm algorithm) {
        if (algorithm == DividerAlgorithm::Restoring) {
            return "Restoring";
        }
        return "Non-restoring";
    }
};
}
