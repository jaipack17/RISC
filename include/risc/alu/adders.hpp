#pragma once

#include <array>
#include <cstdint>

#include "risc/alu/alu_types.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::alu {

class Adders {
private:
    static constexpr unsigned WIDTH = isa::WORD_BITS;

    // 1. Ripple Carry Adder (Slides 9-10)
    // Sequentially ripples carry through all 32 single-bit full adders: O(n) delay
    static AdderResult ripple_carry(Word a, Word b, bool carry_in) {
        Word sum = 0;
        bool carry = carry_in;
        bool carry_into_msb = false;

        for (unsigned bit = 0; bit < WIDTH; ++bit) {
            const bool a_bit = ((a >> bit) & 1u) != 0;
            const bool b_bit = ((b >> bit) & 1u) != 0;
            const bool p = a_bit ^ b_bit;
            const bool g = a_bit && b_bit;

            if (bit == WIDTH - 1) {
                carry_into_msb = carry;
            }

            if (p ^ carry) {
                sum |= Word{1} << bit;
            }
            carry = g || (p && carry);
        }

        const bool overflow = carry_into_msb ^ carry;
        return {sum, carry, overflow};
    }

    // 2. Carry Select Adder (Slides 12-14)
    // Divides 32 bits into blocks of size k = 4 (8 blocks total).
    // Blocks 1-7 compute dual sums assuming Cin=0 and Cin=1 in parallel,
    // selecting the correct sum via a 2:1 mux when the block carry arrives: O(sqrt(n)) delay
    static AdderResult carry_select(Word a, Word b, bool carry_in) {
        constexpr unsigned K = 4; // Block size
        constexpr unsigned NUM_BLOCKS = WIDTH / K; // 8 blocks

        Word sum = 0;
        bool current_carry = carry_in;
        bool carry_into_msb = false;

        for (unsigned blk = 0; blk < NUM_BLOCKS; ++blk) {
            const unsigned shift = blk * K;
            const Word mask = (Word{1} << K) - 1u;
            const Word a_blk = (a >> shift) & mask;
            const Word b_blk = (b >> shift) & mask;

            if (blk == 0) {
                // Block 0: standard ripple carry with external carry_in
                bool blk_carry = current_carry;
                Word blk_sum = 0;
                for (unsigned i = 0; i < K; ++i) {
                    const bool a_bit = ((a_blk >> i) & 1u) != 0;
                    const bool b_bit = ((b_blk >> i) & 1u) != 0;
                    const bool p = a_bit ^ b_bit;
                    const bool g = a_bit && b_bit;
                    if (p ^ blk_carry) {
                        blk_sum |= Word{1} << i;
                    }
                    blk_carry = g || (p && blk_carry);
                }
                sum |= blk_sum << shift;
                current_carry = blk_carry;
            } else {
                // Speculative RCA 0 (assuming Cin = 0)
                Word sum0 = 0;
                bool carry0 = false;
                for (unsigned i = 0; i < K; ++i) {
                    const bool a_bit = ((a_blk >> i) & 1u) != 0;
                    const bool b_bit = ((b_blk >> i) & 1u) != 0;
                    const bool p = a_bit ^ b_bit;
                    const bool g = a_bit && b_bit;
                    if (p ^ carry0) {
                        sum0 |= Word{1} << i;
                    }
                    carry0 = g || (p && carry0);
                }

                // Speculative RCA 1 (assuming Cin = 1)
                Word sum1 = 0;
                bool carry1 = true;
                for (unsigned i = 0; i < K; ++i) {
                    const bool a_bit = ((a_blk >> i) & 1u) != 0;
                    const bool b_bit = ((b_blk >> i) & 1u) != 0;
                    const bool p = a_bit ^ b_bit;
                    const bool g = a_bit && b_bit;
                    if (p ^ carry1) {
                        sum1 |= Word{1} << i;
                    }
                    carry1 = g || (p && carry1);
                }

                // 2:1 Multiplexer selection based on actual incoming carry
                const Word selected_sum = current_carry ? sum1 : sum0;
                sum |= selected_sum << shift;

                if (blk == NUM_BLOCKS - 1) {
                    // Track carry into MSB for overflow check
                    // MSB is at bit K-1 inside this block
                    bool c_msb0 = false;
                    bool c_msb1 = true;
                    for (unsigned i = 0; i < K - 1; ++i) {
                        const bool a_bit = ((a_blk >> i) & 1u) != 0;
                        const bool b_bit = ((b_blk >> i) & 1u) != 0;
                        c_msb0 = (a_bit && b_bit) || ((a_bit ^ b_bit) && c_msb0);
                        c_msb1 = (a_bit && b_bit) || ((a_bit ^ b_bit) && c_msb1);
                    }
                    carry_into_msb = current_carry ? c_msb1 : c_msb0;
                }

                current_carry = current_carry ? carry1 : carry0;
            }
        }

        const bool overflow = carry_into_msb ^ current_carry;
        return {sum, current_carry, overflow};
    }

    // 3. Tree Carry Lookahead Adder (CLA) / Parallel Prefix Tree (Slides 16-26)
    // Generates Gi = Ai * Bi, Pi = Ai ^ Bi and evaluates prefix carry lookahead in O(log n) time.
    static AdderResult carry_lookahead(Word a, Word b, bool carry_in) {
        std::array<bool, WIDTH> bit_propagate{};
        std::array<bool, WIDTH> group_propagate{};
        std::array<bool, WIDTH> group_generate{};

        // Bit-level generate and propagate (Slide 16-17)
        for (unsigned bit = 0; bit < WIDTH; ++bit) {
            const bool a_bit = ((a >> bit) & 1u) != 0;
            const bool b_bit = ((b >> bit) & 1u) != 0;
            bit_propagate[bit] = a_bit ^ b_bit;
            group_propagate[bit] = bit_propagate[bit];
            group_generate[bit] = a_bit && b_bit;
        }

        // Parallel prefix carry lookahead tree: 5 levels for 32 bits (Slides 20-25)
        for (unsigned distance = 1; distance < WIDTH; distance <<= 1) {
            const auto prev_g = group_generate;
            const auto prev_p = group_propagate;
            for (unsigned bit = distance; bit < WIDTH; ++bit) {
                group_generate[bit] = prev_g[bit] || (prev_p[bit] && prev_g[bit - distance]);
                group_propagate[bit] = prev_p[bit] && prev_p[bit - distance];
            }
        }

        // Carry derivation and sum calculation
        Word sum = 0;
        bool carry = carry_in;
        bool carry_into_msb = false;

        for (unsigned bit = 0; bit < WIDTH; ++bit) {
            if (bit == WIDTH - 1) {
                carry_into_msb = carry;
            }
            if (bit_propagate[bit] ^ carry) {
                sum |= Word{1} << bit;
            }
            // Carry for next bit is G_{bit:0} + P_{bit:0} * carry_in
            carry = group_generate[bit] || (group_propagate[bit] && carry_in);
        }

        const bool overflow = carry_into_msb ^ carry;
        return {sum, carry, overflow};
    }

public:
    static AdderResult add(Word a, Word b, bool carry_in, AdderAlgorithm algorithm) {
        switch (algorithm) {
            case AdderAlgorithm::RippleCarry:
                return ripple_carry(a, b, carry_in);
            case AdderAlgorithm::CarrySelect:
                return carry_select(a, b, carry_in);
            case AdderAlgorithm::CarryLookahead:
            default:
                return carry_lookahead(a, b, carry_in);
        }
    }

    static constexpr AdderCharacteristics characteristics(AdderAlgorithm algorithm) {
        switch (algorithm) {
            case AdderAlgorithm::RippleCarry:
                return {"Ripple-Carry Adder", 32, 32, 64.0};
            case AdderAlgorithm::CarrySelect:
                return {"Carry-Select Adder (k=4)", 12, 56, 24.0};
            case AdderAlgorithm::CarryLookahead:
            default:
                return {"Tree Carry-Lookahead Adder (Kogge-Stone Prefix)", 5, 129, 8.0};
        }
    }
};

} // namespace risc::alu
