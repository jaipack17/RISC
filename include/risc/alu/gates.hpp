#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "risc/isa/isa_config.hpp"

namespace risc::alu {

// One wire: its value, and when it settles (in gate delays after the step started)
struct Bit {
    bool value = false;
    unsigned time = 0;
};

// The wires of a number, bit 0 first
using Bus = std::vector<Bit>;

// The gates an algorithm uses, built while it computes, so its delay and size are counted from the
// same code that gives the answer. The unit-gate model: a 2-input AND or OR costs 1 gate delay and
// 1 unit of area, an XOR 2 and 2, a 2:1 multiplexer 2 and 3, and an inverter nothing.
// An iterative unit (multiplier, divider) runs one step between two clock() calls: its registers
// hold the step's results, so every step starts again at time 0.
class Circuit {
private:
    unsigned step_delay = 0; // when the latest signal of this step settled
    unsigned step_area = 0;

    Bit gate(bool value, unsigned inputs_time, unsigned delay, unsigned size) {
        Bit out{value, inputs_time + delay};
        step_area += size;
        step_delay = std::max(step_delay, out.time);
        return out;
    }

public:
    unsigned microcycle_length = 0; // gate delays in one µcycle (0: do not count µcycles)
    unsigned steps = 0;
    unsigned microcycles = 0;
    unsigned delay = 0; // of the longest step
    unsigned area = 0;  // of the largest step (every step reuses the same hardware)

    Bit and_gate(Bit a, Bit b) {
        return gate(a.value && b.value, std::max(a.time, b.time), 1, 1);
    }

    Bit or_gate(Bit a, Bit b) {
        return gate(a.value || b.value, std::max(a.time, b.time), 1, 1);
    }

    Bit xor_gate(Bit a, Bit b) {
        return gate(a.value != b.value, std::max(a.time, b.time), 2, 2);
    }

    Bit not_gate(Bit a) {
        return Bit{!a.value, a.time};
    }

    // if1 when select is 1, else if0
    Bit mux(Bit select, Bit if0, Bit if1) {
        bool value = if0.value;
        if (select.value) {
            value = if1.value;
        }
        return gate(value, std::max({select.time, if0.time, if1.time}), 2, 3);
    }

    // A balanced tree of 2-input ANDs
    Bit and_tree(std::vector<Bit> bits) {
        while (bits.size() > 1) {
            std::vector<Bit> next;
            for (unsigned i = 0; i + 1 < bits.size(); i += 2) {
                next.push_back(and_gate(bits[i], bits[i + 1]));
            }
            if (bits.size() % 2 == 1) {
                next.push_back(bits.back());
            }
            bits = next;
        }
        return bits[0];
    }

    // A balanced tree of 2-input ORs
    Bit or_tree(std::vector<Bit> bits) {
        while (bits.size() > 1) {
            std::vector<Bit> next;
            for (unsigned i = 0; i + 1 < bits.size(); i += 2) {
                next.push_back(or_gate(bits[i], bits[i + 1]));
            }
            if (bits.size() % 2 == 1) {
                next.push_back(bits.back());
            }
            bits = next;
        }
        return bits[0];
    }

    // End of a step: its results are stored in registers. A step takes at least one µcycle.
    void clock() {
        steps++;
        delay = std::max(delay, step_delay);
        area = std::max(area, step_area);
        if (microcycle_length > 0) {
            unsigned cycles = (step_delay + microcycle_length - 1) / microcycle_length;
            if (cycles == 0) {
                cycles = 1;
            }
            microcycles += cycles;
        }
        step_delay = 0;
        step_area = 0;
    }
};

// A register's outputs: settled when the step starts
inline Bus to_bus(std::uint64_t value, unsigned width = isa::WORD_BITS) {
    Bus bus(width);
    for (unsigned i = 0; i < width; i++) {
        bus[i].value = ((value >> i) & 1u) != 0;
    }
    return bus;
}

inline std::uint64_t to_number(const Bus& bus) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < bus.size(); i++) {
        if (bus[i].value) {
            value |= std::uint64_t{1} << i;
        }
    }
    return value;
}

inline Bit constant(bool value) {
    return Bit{value, 0};
}
}
