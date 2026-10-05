#pragma once

#include <array>

#include "risc/isa/isa_config.hpp"

namespace risc::micro {

// Figure 9.25: for mswitch, maps the opcode in ir to the µaddress where that instruction's microcode starts
class SwitchUnit {
private:
    std::array<unsigned, 1u << isa::OPCODE_BITS> start{}; // one entry per opcode

public:
    // Filled in when the microcode is loaded
    void set_start(isa::Opcode opcode, unsigned address) {
        start[unsigned(opcode)] = address;
    }

    unsigned get_start_address(isa::Word ir) const {
        unsigned opcode = (ir >> isa::OPCODE_POS) & ((1u << isa::OPCODE_BITS) - 1);
        return start[opcode];
    }
};
}
