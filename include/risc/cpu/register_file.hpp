#pragma once

#include <array>

#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"

namespace risc::cpu {

struct ReadPort {
    unsigned address = 0; // A
    isa::Word data = 0; // D
};

struct WritePort {
    bool enable = false; // E
    unsigned address = 0; // A
    isa::Word data = 0; // D
};

// The 16 general-purpose registers r0-r15
class RegisterFile {
private:
    std::array<isa::Word, isa::NUM_REGS> regs;
public:
    ReadPort read_port1;
    ReadPort read_port2;
    WritePort write_port;

    RegisterFile() {
        reset();
    }

    // All registers 0, except sp which starts at the top of the empty stack
    void reset() {
        regs.fill(0);
        regs[isa::REG_SP] = machine::INITIAL_SP;
        read_port1 = ReadPort{};
        read_port2 = ReadPort{};
        write_port = WritePort{};
    }

    // Put the selected registers on the D pins of both read ports
    void read() {
        read_port1.data = regs[read_port1.address];
        read_port2.data = regs[read_port2.address];
    }

    // Store D into register A, only if E is set
    void write() {
        if (write_port.enable) {
            regs[write_port.address] = write_port.data;
        }
    }

    // For debugger and visualizer
    isa::Word value(unsigned index) const {
        return regs[index];
    }
};
}
