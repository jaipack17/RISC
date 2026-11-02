#pragma once

#include <cstdint>

#include "risc/isa/isa_config.hpp"

namespace risc::alu {

using Word  = isa::Word;
using SWord = isa::SWord;

enum class AdderAlgorithm : std::uint8_t {
    RippleCarry,            // Slide 10: O(n) delay
    CarrySelect,            // Slides 12-14: O(sqrt(n)) delay, k=4
    HierarchicalGroupedCLA, // Slides 18-25: 4-bit block CLA with hierarchical lookahead
    PrefixTreeCLA,          // Kogge-Stone parallel prefix tree (log2(n) = 5 levels)
    CarryLookahead = HierarchicalGroupedCLA, // Primary choice: Hierarchical Grouped CLA
};

enum class MultiplierAlgorithm : std::uint8_t {
    ShiftAdd,    // Slides 27-29: Unsigned shift-and-add with sign handling
    BoothRadix2, // Slides 30-31: Booth's algorithm checking bit-pairs (Q0, Q-1)
    BoothRadix4, // Modified Booth radix-4: 16 partial products
};

enum class DividerAlgorithm : std::uint8_t {
    Restoring,    // Slides 33-34: Shift left, trial subtract, conditional restore
    NonRestoring, // Smruti Sarangi: Add/subtract based on running remainder sign
};

enum class ShiftOp : std::uint8_t {
    Lsl, // Logical shift left (0-fill)
    Lsr, // Logical shift right (0-fill)
    Asr, // Arithmetic shift right (sign-fill)
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

// Structural metrics for reporting and evaluation (Assignment item 5)
struct AdderCharacteristics {
    const char* name = "";
    unsigned logic_depth = 0;
    unsigned operator_nodes = 0;
    double estimated_fo4 = 0.0;
};

struct MultiplierCharacteristics {
    const char* name = "";
    unsigned cycles = 0;
    unsigned partial_products = 0;
    double estimated_fo4_per_step = 0.0;
};

struct DividerCharacteristics {
    const char* name = "";
    unsigned cycles = 0;
    bool conditional_restore = false;
};

} // namespace risc::alu
