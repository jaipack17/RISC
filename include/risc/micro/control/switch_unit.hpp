#pragma once

#include <array>

#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::micro {

// Figure 9.25: for mswitch, maps the opcode in ir to the µaddress where that instruction's microcode starts
class SwitchUnit {
private:
    std::array<unsigned, 1u << isa::OPCODE_BITS> start{}; // one entry per opcode
    std::array<bool, 1u << isa::OPCODE_BITS> has_entry{}; // false = no microcode for that opcode

public:
    // Filled in when the microcode is loaded
    void set_start(isa::Opcode opcode, unsigned address) {
        start[unsigned(opcode)] = address;
        has_entry[unsigned(opcode)] = true;
    }

    unsigned get_start_address(isa::Word ir) const {
        unsigned opcode = (ir >> isa::OPCODE_POS) & ((1u << isa::OPCODE_BITS) - 1);
        if (!has_entry[opcode]) {
            throw CpuException{isa::Exception::IllegalInstruction, 0};
        }
        return start[opcode];
    }
};
}
