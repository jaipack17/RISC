#pragma once

#include <cstdint>

#include "risc/isa/isa_config.hpp"

namespace risc::alu {

using Word  = isa::Word;
using SWord = isa::SWord;

enum class AdderAlgorithm : std::uint8_t {
    RippleCarry,            // Slide 10: the carry ripples through 32 full adders, O(n)
    CarrySelect,            // Slides 12-14: blocks of 4 add for carry-in 0 and 1, a mux picks, O(sqrt n)
    HierarchicalGroupedCLA, // Slides 18-25: lookahead over groups of 4, then groups of groups, O(log n)
    PrefixTreeCLA,          // Kogge-Stone parallel prefix tree, log2(32) = 5 levels
};

enum class MultiplierAlgorithm : std::uint8_t {
    ShiftAdd,    // Slides 27-29: on the magnitudes, adds M when the multiplier bit is 1, then fixes the sign
    BoothRadix2, // Slides 30-31: looks at (Q0, Q-1), adds or subtracts M, 32 steps
    BoothRadix4, // Modified Booth: looks at 3 bits, adds 0, ±M or ±2M, 16 steps
};

enum class DividerAlgorithm : std::uint8_t {
    Restoring,    // Slides 33-34: subtract M, and add it back when the result is negative
    NonRestoring, // Subtract or add M by the sign of the remainder, one correction at the end
};

enum class ShiftOp : std::uint8_t {
    Lsl, // Logical shift left (0-fill)
    Lsr, // Logical shift right (0-fill)
    Asr, // Arithmetic shift right (sign-fill)
};

// The algorithm each ALU unit uses
struct AluConfig {
    AdderAlgorithm adder = AdderAlgorithm::PrefixTreeCLA;
    MultiplierAlgorithm multiplier = MultiplierAlgorithm::BoothRadix4;
    DividerAlgorithm divider = DividerAlgorithm::NonRestoring;
};

struct AdderResult {
    Word sum = 0;
    bool carry_out = false;
    bool overflow = false;
};

struct DivisionResult {
    Word quotient = 0;
    Word remainder = 0;
};

// Counted from the gates (see gates.hpp), not typed in
struct AdderCharacteristics {
    const char* name = "";
    unsigned delay = 0;               // gate delays, a and b to the sum
    unsigned area = 0;                // unit-gate area
    unsigned compare_delay = 0;       // cmp through the ALU: the XORs that make it subtract, the adder, the flags
    unsigned add_microcycles = 0;     // an add or sub on the ALU
    unsigned compare_microcycles = 0; // a cmp on the ALU
};

struct IterativeCharacteristics {
    const char* name = "";
    unsigned step_delay = 0;          // gate delays in the longest step
    unsigned area = 0;                // unit-gate area of the hardware one step uses
    unsigned min_microcycles = 0;     // over the sample operands
    unsigned max_microcycles = 0;
    double average_microcycles = 0.0; // over random operands
};
}
