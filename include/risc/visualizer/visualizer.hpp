#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "risc/cpu/cpu.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/memory/memory.hpp"

namespace risc::visualizer {

// Text form of an instruction for display: a branch shows its offset and target (see disassembler/)
std::string disassemble(isa::Word pc, isa::Word inst);

// Draws the CPU state after every cycle, redrawn in place in the terminal.
// Only reads the CPU and memory, never changes them.
class Visualizer {
private:
    bool first_draw = true;
    std::array<isa::Word, isa::NUM_REGS> previous_registers{};
    std::uint8_t previous_flags = 0;
    isa::Word memory_window = machine::DATA_BASE; // first address shown in the memory view

public:
    void draw(const cpu::CPU& cpu, const memory::Memory& memory);
};
}
