#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "risc/alu/adders.hpp"
#include "risc/alu/alu_types.hpp"
#include "risc/alu/dividers.hpp"
#include "risc/alu/gates.hpp"
#include "risc/alu/multipliers.hpp"
#include "risc/alu/shifter.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::alu {

struct ALUFlags {
    bool E = false;  // Equal
    bool GT = false; // Signed greater than: (N == V) && !E
    bool C = false;  // Carry
    bool V = false;  // Signed overflow
};

// cmp: the adder subtracts, which gives N and V for GT; E comes from an XOR per bit and a NOR
// tree beside the adder
inline void compare_gates(Circuit& c, Word a, Word b, AdderAlgorithm adder, ALUFlags& flags) {
    Bus a_bus = to_bus(a);
    Bus b_bus = to_bus(b);
    AdderOutput difference = Adders::add_or_subtract(c, a_bus, b_bus, constant(true), adder);

    Bus differs;
    for (unsigned i = 0; i < isa::WORD_BITS; i++) {
        differs.push_back(c.xor_gate(a_bus[i], b_bus[i]));
    }
    Bit equal = c.not_gate(c.or_tree(differs));

    // V: a and ~b have the same sign and the difference has the other one
    unsigned msb = isa::WORD_BITS - 1;
    Bit n = difference.sum[msb];
    Bit same_signs = c.xor_gate(a_bus[msb], b_bus[msb]); // a and ~b have the same sign
    Bit overflow = c.and_gate(same_signs, c.xor_gate(a_bus[msb], n));
    Bit greater = c.and_gate(c.not_gate(equal), c.not_gate(c.xor_gate(n, overflow)));

    flags.E = equal.value;
    flags.GT = greater.value;
    flags.C = difference.carry_out.value;
    flags.V = overflow.value;
}

// Operand pairs that make every unit take each kind of step (add, subtract, restore, negate)
inline std::vector<std::pair<Word, Word>> sample_operands(unsigned random_pairs) {
    std::vector<Word> edges{0, 1, 3, 7, 0xFFFFFFFF, 0xFFFFFFF9, 0x7FFFFFFF, 0x80000000, 0x5A5A5A5B, 12345, 0xFFFE1DC0};
    std::vector<std::pair<Word, Word>> pairs;
    for (Word a : edges) {
        for (Word b : edges) {
            pairs.push_back({a, b});
        }
    }
    std::mt19937 random(201);
    for (unsigned i = 0; i < random_pairs; i++) {
        pairs.push_back({Word(random()), Word(random())});
    }
    return pairs;
}

inline AdderAlgorithm fastest_adder() {
    AdderAlgorithm best = AdderAlgorithm::RippleCarry;
    for (AdderAlgorithm adder : {AdderAlgorithm::CarrySelect, AdderAlgorithm::HierarchicalGroupedCLA, AdderAlgorithm::PrefixTreeCLA}) {
        if (Adders::characteristics(adder).delay < Adders::characteristics(best).delay) {
            best = adder;
        }
    }
    return best;
}

// The µcycle is set so that, with the fastest adder, every ALU operation and every step of the
// multipliers and dividers fits in one µcycle: the longest of them, in gate delays
inline unsigned measure_microcycle_length() {
    AdderAlgorithm adder = fastest_adder();
    Circuit c;
    // Negative operands (sign fixes), Booth adds and subtracts, radix-4 doubles, restores and corrections
    std::vector<std::pair<Word, Word>> pairs{{0xFFFFFFF9, 0x5A5A5A5B}, {0x7FFFFFFF, 0xFFFFFFF9}, {12345, 7}, {0x80000000, 3}};
    for (const std::pair<Word, Word>& pair : pairs) {
        Word a = pair.first;
        Word b = pair.second;
        ALUFlags flags;
        compare_gates(c, a, b, adder, flags);
        c.clock();
        Shifter::shift(c, a, b, ShiftOp::Asr);
        c.clock();
        for (MultiplierAlgorithm m : {MultiplierAlgorithm::ShiftAdd, MultiplierAlgorithm::BoothRadix2, MultiplierAlgorithm::BoothRadix4}) {
            Multipliers::multiply(c, a, b, m, adder);
        }
        if (b != 0) {
            for (DividerAlgorithm d : {DividerAlgorithm::Restoring, DividerAlgorithm::NonRestoring}) {
                Dividers::divide(c, a, b, d, adder);
            }
        }
    }
    return c.delay;
}

inline unsigned microcycle_length() {
    static const unsigned length = measure_microcycle_length();
    return length;
}

// The ALU of the processors. Every operation runs on gates and leaves in microcycles how many
// µcycles it took: a single pass needs ceil(delay / µcycle) of them, a multiply or divide one or
// more per step.
class OptimizedALU {
private:
    AluConfig config;

    Circuit new_circuit() const {
        Circuit c;
        c.microcycle_length = microcycle_length();
        return c;
    }

    Word add_or_subtract(Word a, Word b, bool subtract, ALUFlags& flags) {
        Circuit c = new_circuit();
        AdderOutput out = Adders::add_or_subtract(c, to_bus(a), to_bus(b), constant(subtract), config.adder);
        c.clock();
        microcycles = c.microcycles;

        Word result = Word(to_number(out.sum));
        Word b_in = b;
        if (subtract) {
            b_in = ~b;
        }
        bool a_negative = (a >> (isa::WORD_BITS - 1)) != 0;
        bool b_negative = (b_in >> (isa::WORD_BITS - 1)) != 0;
        bool negative = (result >> (isa::WORD_BITS - 1)) != 0;
        flags.E = result == 0;
        flags.C = out.carry_out.value;
        flags.V = a_negative == b_negative && negative != a_negative;
        flags.GT = !flags.E && negative == flags.V;
        return result;
    }

public:
    unsigned microcycles = 1; // how long the last operation took

    explicit OptimizedALU(AluConfig config = AluConfig{}) : config(config) {}

    void configure(const AluConfig& new_config) {
        config = new_config;
    }

    const AluConfig& configuration() const {
        return config;
    }

    void select_adder(AdderAlgorithm algorithm) {
        config.adder = algorithm;
    }

    void select_multiplier(MultiplierAlgorithm algorithm) {
        config.multiplier = algorithm;
    }

    void select_divider(DividerAlgorithm algorithm) {
        config.divider = algorithm;
    }

    Word add(Word a, Word b, ALUFlags& flags) {
        return add_or_subtract(a, b, false, flags);
    }

    Word sub(Word a, Word b, ALUFlags& flags) {
        return add_or_subtract(a, b, true, flags);
    }

    void cmp(Word a, Word b, ALUFlags& flags) {
        Circuit c = new_circuit();
        compare_gates(c, a, b, config.adder, flags);
        c.clock();
        microcycles = c.microcycles;
    }

    Word mul(Word a, Word b) {
        Circuit c = new_circuit();
        Word product = Multipliers::multiply(c, a, b, config.multiplier, config.adder);
        microcycles = c.microcycles;
        return product;
    }

    Word div(Word a, Word b) {
        Circuit c = new_circuit();
        DivisionResult result = Dividers::divide(c, a, b, config.divider, config.adder);
        microcycles = c.microcycles;
        return result.quotient;
    }

    Word mod(Word a, Word b) {
        Circuit c = new_circuit();
        DivisionResult result = Dividers::divide(c, a, b, config.divider, config.adder);
        microcycles = c.microcycles;
        return result.remainder;
    }

    Word shift(Word a, Word b, ShiftOp op) {
        Circuit c = new_circuit();
        Word result = Shifter::shift(c, a, b, op);
        c.clock();
        microcycles = c.microcycles;
        return result;
    }

    static Word bitwise_and(Word a, Word b) {
        return a & b;
    }

    static Word bitwise_or(Word a, Word b) {
        return a | b;
    }

    static Word bitwise_xor(Word a, Word b) {
        return a ^ b;
    }

    static Word bitwise_not(Word b) {
        return ~b;
    }
};

// Measurements for the report, all counted from the gates

inline unsigned microcycles_for(unsigned delay) {
    unsigned cycles = (delay + microcycle_length() - 1) / microcycle_length();
    if (cycles == 0) {
        cycles = 1;
    }
    return cycles;
}

inline AdderCharacteristics measure_adder(AdderAlgorithm adder) {
    AdderCharacteristics result = Adders::characteristics(adder);
    Circuit add;
    Adders::add_or_subtract(add, to_bus(0), to_bus(0), constant(true), adder);
    add.clock();
    Circuit compare;
    ALUFlags flags;
    compare_gates(compare, 0, 0, adder, flags);
    compare.clock();
    result.compare_delay = compare.delay;
    result.add_microcycles = microcycles_for(add.delay);
    result.compare_microcycles = microcycles_for(compare.delay);
    return result;
}

inline IterativeCharacteristics measure_iterative(bool is_multiplier, MultiplierAlgorithm multiplier, DividerAlgorithm divider, AdderAlgorithm adder) {
    IterativeCharacteristics result;
    std::uint64_t total = 0;
    unsigned runs = 0;
    for (const std::pair<Word, Word>& pair : sample_operands(300)) {
        if (!is_multiplier && pair.second == 0) {
            continue;
        }
        Circuit c;
        c.microcycle_length = microcycle_length();
        if (is_multiplier) {
            Multipliers::multiply(c, pair.first, pair.second, multiplier, adder);
        } else {
            Dividers::divide(c, pair.first, pair.second, divider, adder);
        }
        if (runs == 0 || c.microcycles < result.min_microcycles) {
            result.min_microcycles = c.microcycles;
        }
        if (c.microcycles > result.max_microcycles) {
            result.max_microcycles = c.microcycles;
        }
        if (c.delay > result.step_delay) {
            result.step_delay = c.delay;
        }
        if (c.area > result.area) {
            result.area = c.area;
        }
        total += c.microcycles;
        runs++;
    }
    result.average_microcycles = double(total) / double(runs);
    return result;
}

inline IterativeCharacteristics measure_multiplier(MultiplierAlgorithm multiplier, AdderAlgorithm adder) {
    IterativeCharacteristics result = measure_iterative(true, multiplier, DividerAlgorithm::Restoring, adder);
    result.name = Multipliers::name(multiplier);
    return result;
}

inline IterativeCharacteristics measure_divider(DividerAlgorithm divider, AdderAlgorithm adder) {
    IterativeCharacteristics result = measure_iterative(false, MultiplierAlgorithm::ShiftAdd, divider, adder);
    result.name = Dividers::name(divider);
    return result;
}
}
