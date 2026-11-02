#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "risc/alu/alu.hpp"

using namespace risc;
using namespace risc::alu;

static int tests_run = 0;
static int tests_passed = 0;

#define EXPECT_EQ(actual, expected) do { \
    ++tests_run; \
    if ((actual) == (expected)) { \
        ++tests_passed; \
    } else { \
        std::cerr << "[FAIL] Line " << __LINE__ << ": expected " \
                  << (expected) << ", got " << (actual) << '\n'; \
    } \
} while (0)

#define EXPECT_TRUE(cond) do { \
    ++tests_run; \
    if (cond) { \
        ++tests_passed; \
    } else { \
        std::cerr << "[FAIL] Line " << __LINE__ << ": expected true for (" #cond ")\n"; \
    } \
} while (0)

#define EXPECT_FALSE(cond) do { \
    ++tests_run; \
    if (!(cond)) { \
        ++tests_passed; \
    } else { \
        std::cerr << "[FAIL] Line " << __LINE__ << ": expected false for (" #cond ")\n"; \
    } \
} while (0)

void test_adder_algorithms() {
    std::cout << "Testing Adder Algorithms (Ripple-Carry, Carry-Select, Grouped CLA, Prefix Tree CLA)...\n";
    OptimizedALU alu;

    const std::vector<AdderAlgorithm> adders = {
        AdderAlgorithm::RippleCarry,
        AdderAlgorithm::CarrySelect,
        AdderAlgorithm::HierarchicalGroupedCLA,
        AdderAlgorithm::PrefixTreeCLA
    };

    for (auto algo : adders) {
        alu.select_adder(algo);
        ALUFlags flags;

        // Basic addition
        Word sum = alu.add(15, 25, flags);
        EXPECT_EQ(sum, 40u);
        EXPECT_FALSE(flags.E);
        EXPECT_FALSE(flags.C);
        EXPECT_FALSE(flags.V);
        EXPECT_TRUE(flags.GT);

        // Zero addition
        sum = alu.add(0, 0, flags);
        EXPECT_EQ(sum, 0u);
        EXPECT_TRUE(flags.E);
        EXPECT_FALSE(flags.GT);

        // Unsigned carry out without overflow
        sum = alu.add(0xFFFFFFFF, 1, flags);
        EXPECT_EQ(sum, 0u);
        EXPECT_TRUE(flags.E);
        EXPECT_TRUE(flags.C);
        EXPECT_FALSE(flags.V);

        // Signed positive overflow: 0x7FFFFFFF + 1 = 0x80000000 (-2147483648)
        sum = alu.add(0x7FFFFFFF, 1, flags);
        EXPECT_EQ(sum, 0x80000000u);
        EXPECT_TRUE(flags.V);
        EXPECT_FALSE(flags.C);

        // Signed negative overflow: 0x80000000 + 0x80000000 = 0
        sum = alu.add(0x80000000, 0x80000000, flags);
        EXPECT_EQ(sum, 0u);
        EXPECT_TRUE(flags.V);
        EXPECT_TRUE(flags.C);
    }

    // Exhaustive randomized differential test across all 4 adders
    std::mt19937 rng(42);
    std::uniform_int_distribution<std::uint32_t> dist;
    for (int i = 0; i < 1000; ++i) {
        Word a = dist(rng);
        Word b = dist(rng);
        AdderResult rca = Adders::add(a, b, false, AdderAlgorithm::RippleCarry);
        AdderResult csa = Adders::add(a, b, false, AdderAlgorithm::CarrySelect);
        AdderResult hgcla = Adders::add(a, b, false, AdderAlgorithm::HierarchicalGroupedCLA);
        AdderResult ptcla = Adders::add(a, b, false, AdderAlgorithm::PrefixTreeCLA);

        EXPECT_EQ(rca.sum, csa.sum);
        EXPECT_EQ(csa.sum, hgcla.sum);
        EXPECT_EQ(hgcla.sum, ptcla.sum);
        EXPECT_EQ(rca.carry_out, csa.carry_out);
        EXPECT_EQ(csa.carry_out, hgcla.carry_out);
        EXPECT_EQ(hgcla.carry_out, ptcla.carry_out);
        EXPECT_EQ(rca.overflow, csa.overflow);
        EXPECT_EQ(csa.overflow, hgcla.overflow);
        EXPECT_EQ(hgcla.overflow, ptcla.overflow);
    }
}

void test_subtraction_and_comparison_flags() {
    std::cout << "Testing Subtraction and Signed Comparison Flags (GT trap check)...\n";
    OptimizedALU alu;
    ALUFlags flags;

    // Normal comparison: 50 vs 20
    alu.cmp(50, 20, flags);
    EXPECT_FALSE(flags.E);
    EXPECT_TRUE(flags.GT);
    EXPECT_FALSE(flags.V);

    // Equal comparison
    alu.cmp(100, 100, flags);
    EXPECT_TRUE(flags.E);
    EXPECT_FALSE(flags.GT);
    EXPECT_FALSE(flags.V);

    // Negative vs Positive: -10 vs 5
    alu.cmp(Word(-10), 5, flags);
    EXPECT_FALSE(flags.E);
    EXPECT_FALSE(flags.GT);

    // Critical Two's Complement Overflow Trap:
    // A = +2147483647 (0x7FFFFFFF), B = -2147483648 (0x80000000)
    // Mathematically A > B, but subtraction overflows (result is -1, N=1, V=1).
    // GT = (N == V) && !E correctly evaluates to true!
    alu.cmp(0x7FFFFFFF, 0x80000000, flags);
    EXPECT_FALSE(flags.E);
    EXPECT_TRUE(flags.V);  // overflow occurred!
    EXPECT_TRUE(flags.GT); // but mathematically A > B is correctly preserved!

    // Reverse comparison: -2147483648 vs +2147483647
    alu.cmp(0x80000000, 0x7FFFFFFF, flags);
    EXPECT_FALSE(flags.E);
    EXPECT_TRUE(flags.V);
    EXPECT_FALSE(flags.GT); // correctly false!
}

void test_multiplier_algorithms() {
    std::cout << "Testing Multiplier Algorithms (Shift-Add, Booth Radix-2, Booth Radix-4)...\n";
    OptimizedALU alu;

    const std::vector<MultiplierAlgorithm> multipliers = {
        MultiplierAlgorithm::ShiftAdd,
        MultiplierAlgorithm::BoothRadix2,
        MultiplierAlgorithm::BoothRadix4
    };

    for (auto algo : multipliers) {
        alu.select_multiplier(algo);

        // Pos * Pos
        EXPECT_EQ(alu.mul(12, 10), 120u);
        // Pos * Neg
        EXPECT_EQ(alu.mul(12, Word(-10)), Word(-120));
        // Neg * Pos
        EXPECT_EQ(alu.mul(Word(-12), 10), Word(-120));
        // Neg * Neg
        EXPECT_EQ(alu.mul(Word(-12), Word(-10)), 120u);
        // Zero
        EXPECT_EQ(alu.mul(0, 12345), 0u);
        EXPECT_EQ(alu.mul(12345, 0), 0u);
        // -1 * -1 = 1
        EXPECT_EQ(alu.mul(Word(-1), Word(-1)), 1u);
        // Overflow / modular multiplication
        EXPECT_EQ(alu.mul(0x7FFFFFFF, 2), 0xFFFFFFFEu);
    }

    // Randomized differential testing
    std::mt19937 rng(1337);
    std::uniform_int_distribution<std::int32_t> sdist;
    for (int i = 0; i < 1000; ++i) {
        SWord a = sdist(rng);
        SWord b = sdist(rng);
        Word expected = static_cast<Word>(a * b);

        Word p_shift = Multipliers::multiply(Word(a), Word(b), MultiplierAlgorithm::ShiftAdd);
        Word p_booth2 = Multipliers::multiply(Word(a), Word(b), MultiplierAlgorithm::BoothRadix2);
        Word p_booth4 = Multipliers::multiply(Word(a), Word(b), MultiplierAlgorithm::BoothRadix4);

        EXPECT_EQ(p_shift, expected);
        EXPECT_EQ(p_booth2, expected);
        EXPECT_EQ(p_booth4, expected);
    }
}

void test_divider_algorithms() {
    std::cout << "Testing Divider Algorithms (Restoring vs Non-Restoring)...\n";
    OptimizedALU alu;

    const std::vector<DividerAlgorithm> dividers = {
        DividerAlgorithm::Restoring,
        DividerAlgorithm::NonRestoring
    };

    for (auto algo : dividers) {
        alu.select_divider(algo);

        // Pos / Pos
        EXPECT_EQ(alu.div(100, 7), 14u);
        EXPECT_EQ(alu.mod(100, 7), 2u);

        // Neg / Pos
        EXPECT_EQ(alu.div(Word(-100), 7), Word(-14));
        EXPECT_EQ(alu.mod(Word(-100), 7), Word(-2)); // remainder sign follows dividend

        // Pos / Neg
        EXPECT_EQ(alu.div(100, Word(-7)), Word(-14));
        EXPECT_EQ(alu.mod(100, Word(-7)), 2u); // remainder sign follows dividend

        // Neg / Neg
        EXPECT_EQ(alu.div(Word(-100), Word(-7)), 14u);
        EXPECT_EQ(alu.mod(Word(-100), Word(-7)), Word(-2));

        // Dividend < Divisor
        EXPECT_EQ(alu.div(5, 10), 0u);
        EXPECT_EQ(alu.mod(5, 10), 5u);

        // Boundary overflow: INT_MIN / -1
        Word int_min = 0x80000000u;
        EXPECT_EQ(alu.div(int_min, Word(-1)), int_min);
        EXPECT_EQ(alu.mod(int_min, Word(-1)), 0u);

        // Division by Zero check
        bool caught_div_zero = false;
        try {
            alu.div(42, 0);
        } catch (const CpuException& e) {
            caught_div_zero = (e.kind == isa::Exception::DivideByZero);
        }
        EXPECT_TRUE(caught_div_zero);
    }
}

void test_barrel_shifter() {
    std::cout << "Testing 5-Stage Logarithmic Barrel Shifter...\n";
    OptimizedALU alu;

    Word val = 0x12345678;
    EXPECT_EQ(alu.shift(val, 0, ShiftOp::Lsl), val);
    EXPECT_EQ(alu.shift(val, 4, ShiftOp::Lsl), 0x23456780u);
    EXPECT_EQ(alu.shift(val, 16, ShiftOp::Lsl), 0x56780000u);
    EXPECT_EQ(alu.shift(val, 4, ShiftOp::Lsr), 0x01234567u);
    EXPECT_EQ(alu.shift(val, 16, ShiftOp::Lsr), 0x00001234u);

    // Shift amount masking (only low 5 bits used)
    EXPECT_EQ(alu.shift(val, 32, ShiftOp::Lsl), val); // 32 & 31 == 0
    EXPECT_EQ(alu.shift(val, 36, ShiftOp::Lsl), 0x23456780u); // 36 & 31 == 4

    // Arithmetic Shift Right with sign extension
    Word neg = 0x80000008;
    EXPECT_EQ(alu.shift(neg, 1, ShiftOp::Asr), 0xC0000004u);
    EXPECT_EQ(alu.shift(neg, 4, ShiftOp::Asr), 0xF8000000u);
}

void print_alu_characteristics() {
    std::cout << "\n============================================================\n";
    std::cout << "        RISC201 ALU Structural Characterization Report       \n";
    std::cout << "============================================================\n";

    std::cout << "\n[1] 32-bit Adders / Subtractors:\n";
    std::cout << std::left << std::setw(45) << "Algorithm"
              << std::setw(14) << "Logic Depth"
              << std::setw(14) << "Nodes (FAs)"
              << "FO4 Delay\n";
    std::cout << std::string(82, '-') << '\n';

    const AdderAlgorithm adders[] = {
        AdderAlgorithm::RippleCarry,
        AdderAlgorithm::CarrySelect,
        AdderAlgorithm::HierarchicalGroupedCLA,
        AdderAlgorithm::PrefixTreeCLA
    };
    for (auto a : adders) {
        auto c = Adders::characteristics(a);
        std::cout << std::left << std::setw(45) << c.name
                  << std::setw(14) << c.logic_depth
                  << std::setw(14) << c.operator_nodes
                  << c.estimated_fo4 << " FO4\n";
    }

    std::cout << "\n[2] 32-bit Multipliers:\n";
    std::cout << std::left << std::setw(50) << "Algorithm"
              << std::setw(12) << "Cycles"
              << std::setw(14) << "Part. Prods"
              << "FO4 / step\n";
    std::cout << std::string(84, '-') << '\n';

    const MultiplierAlgorithm mults[] = {
        MultiplierAlgorithm::ShiftAdd,
        MultiplierAlgorithm::BoothRadix2,
        MultiplierAlgorithm::BoothRadix4
    };
    for (auto m : mults) {
        auto c = Multipliers::characteristics(m);
        std::cout << std::left << std::setw(50) << c.name
                  << std::setw(12) << c.cycles
                  << std::setw(14) << c.partial_products
                  << c.estimated_fo4_per_step << " FO4\n";
    }

    std::cout << "\n[3] 32-bit Dividers:\n";
    std::cout << std::left << std::setw(45) << "Algorithm"
              << std::setw(12) << "Cycles"
              << "Conditional Restore\n";
    std::cout << std::string(72, '-') << '\n';

    const DividerAlgorithm divs[] = {
        DividerAlgorithm::Restoring,
        DividerAlgorithm::NonRestoring
    };
    for (auto d : divs) {
        auto c = Dividers::characteristics(d);
        std::cout << std::left << std::setw(45) << c.name
                  << std::setw(12) << c.cycles
                  << (c.conditional_restore ? "Yes (Slide 34)" : "No") << '\n';
    }
    std::cout << "============================================================\n\n";
}

int main() {
    std::cout << "Starting RISC201 ALU Verification Suite...\n\n";

    test_adder_algorithms();
    test_subtraction_and_comparison_flags();
    test_multiplier_algorithms();
    test_divider_algorithms();
    test_barrel_shifter();

    print_alu_characteristics();

    std::cout << "Verification Result: " << tests_passed << " / " << tests_run << " assertions passed.\n";
    if (tests_passed == tests_run) {
        std::cout << ">>> ALL ALU TESTS PASSED SUCCESSFULLY! <<<\n";
        return 0;
    } else {
        std::cerr << ">>> SOME TESTS FAILED! <<<\n";
        return 1;
    }
}
