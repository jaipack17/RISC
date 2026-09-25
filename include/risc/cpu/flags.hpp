#pragma once

#include <cstdint>

namespace risc::cpu {

// The hidden flags register, one bit per flag (FLAG_E, FLAG_GT, FLAG_C, FLAG_V)
struct Flags {
    std::uint8_t bits = 0;

    bool is_set(std::uint8_t flag) const {
        return (bits & flag) != 0;
    }

    void set(std::uint8_t flag, bool value) {
        if (value) {
            bits = bits | flag;
        } else {
            bits = bits & ~flag;
        }
    }
};
}
