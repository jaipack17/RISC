#pragma once

#include <array>

#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/memory/stack_guard.hpp"

namespace risc::four_stage_micro {

// One set of registers with three sides, so OF and EX can read while MW writes
class RegisterFile {
private:
    std::array<isa::Word, isa::NUM_REGS> regs;

public:
    // Read side 1, used by OF
    unsigned readSrc1 = 0;
    isa::Word regVal1 = 0;

    // Read side 2, used by EX
    unsigned readSrc2 = 0;
    isa::Word regVal2 = 0;

    // Write side, used by MW
    unsigned writeSrc = 0;
    isa::Word regData = 0;

    RegisterFile() {
        reset();
    }

    void reset() {
        regs.fill(0);
        regs[isa::REG_SP] = machine::INITIAL_SP;
        readSrc1 = 0;
        regVal1 = 0;
        readSrc2 = 0;
        regVal2 = 0;
        writeSrc = 0;
        regData = 0;
    }

    // <read> on OF's bus
    void read1() {
        regVal1 = regs[readSrc1 % isa::NUM_REGS]; // the src registers are 4 bits
    }

    // <read> on EX's bus
    void read2() {
        regVal2 = regs[readSrc2 % isa::NUM_REGS];
    }

    // <write> on MW's bus. Throws StackOverflow / StackUnderflow if sp would leave the stack.
    void write() {
        unsigned index = writeSrc % isa::NUM_REGS;
        if (index == isa::REG_SP) {
            memory::check_stack_pointer(regData);
        }
        regs[index] = regData;
    }

    // For the debugger and visualizer
    isa::Word value(unsigned index) const {
        return regs[index];
    }
};
}
