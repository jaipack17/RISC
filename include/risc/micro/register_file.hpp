#pragma once

#include <array>
#include <cstdint>

#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"

namespace risc::micro {

// Register file args
enum class RegisterFileOp : std::uint8_t { Nop = 0, Read = 1, Write = 2 };

class RegisterFile {
private:
    std::array<isa::Word, isa::NUM_REGS> regs;

public:
    unsigned regSrc = 0;
    isa::Word regData = 0;
    isa::Word regVal = 0;

    RegisterFile() {
        reset();
    }

    void reset() {
        regs.fill(0);
        regs[isa::REG_SP] = machine::INITIAL_SP;
        regSrc = 0;
        regData = 0;
        regVal = 0;
    }

    void run(RegisterFileOp op) {
        unsigned index = regSrc % isa::NUM_REGS; // regSrc is a 4 bit register
        if (op == RegisterFileOp::Read) {
            regVal = regs[index];
        } else if (op == RegisterFileOp::Write) {
            regs[index] = regData;
        }
    }

    // For the debugger and visualizer
    isa::Word value(unsigned index) const {
        return regs[index];
    }
};
}
