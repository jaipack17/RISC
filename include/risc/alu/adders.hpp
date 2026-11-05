#pragma once

#include <vector>

#include "risc/alu/alu_types.hpp"
#include "risc/alu/gates.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::alu {

struct AdderOutput {
    Bus sum;
    Bit carry_out;
};

// The four adders, built from gates for any width. Each one works out the carry into every bit
// from the bits' generate (g = a·b) and propagate (p = a⊕b) signals; then sum = p ⊕ carry.
class Adders {
private:
    static constexpr unsigned GROUP = 4; // bits in a lookahead group or a carry-select block

    // carries[i] is the carry into bit i, carries[n] the carry out
    static std::vector<Bit> ripple_carries(Circuit& c, const Bus& g, const Bus& p, Bit carry_in) {
        std::vector<Bit> carries{carry_in};
        for (unsigned i = 0; i < g.size(); i++) {
            carries.push_back(c.or_gate(g[i], c.and_gate(p[i], carries[i])));
        }
        return carries;
    }

    // Slide 18 for up to 4 bits: c(i+1) = g(i) + p(i)g(i-1) + ... + p(i)...p(0)c(0), every carry at once
    static std::vector<Bit> group_carries(Circuit& c, const Bus& g, const Bus& p, Bit carry_in) {
        std::vector<Bit> carries{carry_in};
        for (int i = 0; i < int(g.size()); i++) {
            std::vector<Bit> terms;
            for (int j = i; j >= -1; j--) {
                std::vector<Bit> factors;
                if (j >= 0) {
                    factors.push_back(g[j]);
                } else {
                    factors.push_back(carry_in);
                }
                for (int k = j + 1; k <= i; k++) {
                    factors.push_back(p[k]);
                }
                terms.push_back(c.and_tree(factors));
            }
            carries.push_back(c.or_tree(terms));
        }
        return carries;
    }

    // A group generates a carry (G) or passes one through (P)
    static void group_signals(Circuit& c, const Bus& g, const Bus& p, Bit& G, Bit& P) {
        std::vector<Bit> terms;
        for (int j = int(g.size()) - 1; j >= 0; j--) {
            std::vector<Bit> factors{g[j]};
            for (int k = j + 1; k < int(g.size()); k++) {
                factors.push_back(p[k]);
            }
            terms.push_back(c.and_tree(factors));
        }
        G = c.or_tree(terms);
        P = c.and_tree(std::vector<Bit>(p.begin(), p.end()));
    }

    // Hierarchical lookahead: groups of 4 bits give a G and a P each, and the carries into the
    // groups come from the same lookahead one level up (groups of groups), until one group is left
    static std::vector<Bit> lookahead_carries(Circuit& c, const Bus& g, const Bus& p, Bit carry_in) {
        if (g.size() <= GROUP) {
            return group_carries(c, g, p, carry_in);
        }
        Bus group_g;
        Bus group_p;
        for (unsigned start = 0; start < g.size(); start += GROUP) {
            unsigned end = std::min<unsigned>(start + GROUP, g.size());
            Bit G;
            Bit P;
            group_signals(c, Bus(g.begin() + start, g.begin() + end), Bus(p.begin() + start, p.begin() + end), G, P);
            group_g.push_back(G);
            group_p.push_back(P);
        }
        std::vector<Bit> into_group = lookahead_carries(c, group_g, group_p, carry_in);

        std::vector<Bit> carries;
        for (unsigned start = 0, group = 0; start < g.size(); start += GROUP, group++) {
            unsigned end = std::min<unsigned>(start + GROUP, g.size());
            std::vector<Bit> inside = group_carries(c, Bus(g.begin() + start, g.begin() + end),
                                                    Bus(p.begin() + start, p.begin() + end), into_group[group]);
            carries.insert(carries.end(), inside.begin(), inside.end() - 1);
        }
        carries.push_back(into_group.back());
        return carries;
    }

    // Kogge-Stone: level k combines each bit's (G, P) with the one 2^k bits below it, so after
    // log2(n) levels G(i), P(i) cover bits i..0; the carry into bit i+1 is G(i) + P(i)·carry_in
    static std::vector<Bit> prefix_carries(Circuit& c, const Bus& g, const Bus& p, Bit carry_in) {
        Bus G = g;
        Bus P = p;
        for (unsigned distance = 1; distance < g.size(); distance *= 2) {
            Bus next_G = G;
            Bus next_P = P;
            for (unsigned i = distance; i < g.size(); i++) {
                next_G[i] = c.or_gate(G[i], c.and_gate(P[i], G[i - distance]));
                next_P[i] = c.and_gate(P[i], P[i - distance]);
            }
            G = next_G;
            P = next_P;
        }
        std::vector<Bit> carries{carry_in};
        for (unsigned i = 0; i < g.size(); i++) {
            carries.push_back(c.or_gate(G[i], c.and_gate(P[i], carry_in)));
        }
        return carries;
    }

    // Carry select: block 0 ripples from carry_in; every other block ripples twice, for a carry-in of
    // 0 and of 1, and a mux picks the sums and carry when the block's real carry-in arrives
    static AdderOutput carry_select(Circuit& c, const Bus& g, const Bus& p, Bit carry_in) {
        AdderOutput out;
        Bit block_carry = carry_in;
        for (unsigned start = 0; start < g.size(); start += GROUP) {
            unsigned end = std::min<unsigned>(start + GROUP, g.size());
            Bus block_g(g.begin() + start, g.begin() + end);
            Bus block_p(p.begin() + start, p.begin() + end);
            if (start == 0) {
                std::vector<Bit> carries = ripple_carries(c, block_g, block_p, block_carry);
                for (unsigned i = 0; i < block_g.size(); i++) {
                    out.sum.push_back(c.xor_gate(block_p[i], carries[i]));
                }
                block_carry = carries.back();
            } else {
                std::vector<Bit> carries0 = ripple_carries(c, block_g, block_p, constant(false));
                std::vector<Bit> carries1 = ripple_carries(c, block_g, block_p, constant(true));
                for (unsigned i = 0; i < block_g.size(); i++) {
                    Bit sum0 = c.xor_gate(block_p[i], carries0[i]);
                    Bit sum1 = c.xor_gate(block_p[i], carries1[i]);
                    out.sum.push_back(c.mux(block_carry, sum0, sum1));
                }
                block_carry = c.mux(block_carry, carries0.back(), carries1.back());
            }
        }
        out.carry_out = block_carry;
        return out;
    }

public:
    static AdderOutput add(Circuit& c, const Bus& a, const Bus& b, Bit carry_in, AdderAlgorithm algorithm) {
        Bus g;
        Bus p;
        for (unsigned i = 0; i < a.size(); i++) {
            g.push_back(c.and_gate(a[i], b[i]));
            p.push_back(c.xor_gate(a[i], b[i]));
        }
        if (algorithm == AdderAlgorithm::CarrySelect) {
            return carry_select(c, g, p, carry_in);
        }

        std::vector<Bit> carries;
        if (algorithm == AdderAlgorithm::RippleCarry) {
            carries = ripple_carries(c, g, p, carry_in);
        } else if (algorithm == AdderAlgorithm::HierarchicalGroupedCLA) {
            carries = lookahead_carries(c, g, p, carry_in);
        } else {
            carries = prefix_carries(c, g, p, carry_in);
        }
        AdderOutput out;
        for (unsigned i = 0; i < a.size(); i++) {
            out.sum.push_back(c.xor_gate(p[i], carries[i]));
        }
        out.carry_out = carries.back();
        return out;
    }

    // a + b, or a - b = a + ~b + 1 when subtract is 1 (Slide 6): an XOR on every bit of b
    static AdderOutput add_or_subtract(Circuit& c, const Bus& a, const Bus& b, Bit subtract, AdderAlgorithm algorithm) {
        Bus b_in;
        for (const Bit& bit : b) {
            b_in.push_back(c.xor_gate(bit, subtract));
        }
        return add(c, a, b_in, subtract, algorithm);
    }

    static AdderResult add(Word a, Word b, bool carry_in, AdderAlgorithm algorithm) {
        Circuit c;
        AdderOutput out = add(c, to_bus(a), to_bus(b), constant(carry_in), algorithm);
        AdderResult result;
        result.sum = Word(to_number(out.sum));
        result.carry_out = out.carry_out.value;
        // Signed overflow: both inputs have the same sign and the sum has the other one
        bool a_negative = (a >> (isa::WORD_BITS - 1)) != 0;
        bool b_negative = (b >> (isa::WORD_BITS - 1)) != 0;
        bool sum_negative = (result.sum >> (isa::WORD_BITS - 1)) != 0;
        result.overflow = a_negative == b_negative && sum_negative != a_negative;
        return result;
    }

    static const char* name(AdderAlgorithm algorithm) {
        switch (algorithm) {
            case AdderAlgorithm::RippleCarry:
                return "Ripple-carry";
            case AdderAlgorithm::CarrySelect:
                return "Carry-select (blocks of 4)";
            case AdderAlgorithm::HierarchicalGroupedCLA:
                return "Hierarchical carry-lookahead (groups of 4)";
            case AdderAlgorithm::PrefixTreeCLA:
                return "Kogge-Stone prefix tree";
        }
        return "";
    }

    // A 32-bit add, measured: which gates exist and when they settle does not depend on the operands
    static AdderCharacteristics characteristics(AdderAlgorithm algorithm) {
        Circuit c;
        add(c, to_bus(0), to_bus(0), constant(false), algorithm);
        c.clock();
        AdderCharacteristics result;
        result.name = name(algorithm);
        result.delay = c.delay;
        result.area = c.area;
        return result;
    }
};
}
