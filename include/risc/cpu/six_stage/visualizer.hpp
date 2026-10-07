#pragma once

#include <array>
#include <cstdint>

#include "risc/cpu/six_stage/cpu.hpp"
#include "risc/memory/memory.hpp"

namespace risc::cpu::six_stage {

// Terminal view of the six-stage microcoded CPU. It only reads machine state.
class Visualizer {
private:
    bool first_draw = true;
    std::array<isa::Word, isa::NUM_REGS> previous_registers{};
    std::uint8_t previous_flags = 0;
    isa::Word memory_window = machine::DATA_BASE;

public:
    void draw(const CPU& cpu, const memory::Memory& memory);
};

} // namespace risc::cpu::six_stage
