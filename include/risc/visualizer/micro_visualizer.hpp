#pragma once

#include <array>
#include <string>
#include <vector>

#include "risc/isa/isa_config.hpp"
#include "risc/micro/control/micro_control_unit.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/micro/datapath/datapath.hpp"
#include "risc/microassembler/microassembler.hpp"

namespace risc::visualizer {

// Draws the microprogrammed processor after every microcycle, redrawn in place in the terminal.
// Only reads the processor, never changes it.
class MicroVisualizer {
private:
    std::vector<isa::Word> program; // machine code, shown as assembly in the program panel
    std::vector<microassembler::Entry> entries; // switch table: routine names and where they start

    bool first_draw = true;
    std::array<isa::Word, isa::NUM_REGS> previous_registers{};
    std::array<isa::Word, 20> previous_internal{}; // indexed by MicroReg
    std::array<bool, micro::MICROPROGRAM_SIZE> executed{}; // µaddresses run for the current instruction

public:
    MicroVisualizer(const std::vector<isa::Word>& program, const std::vector<microassembler::Entry>& entries);

    // executed_upc: the µaddress just executed. instruction_pc: address of the current instruction.
    void draw(const micro::Datapath& datapath, const micro::MicroControlUnit& control, unsigned executed_upc,
              isa::Word instruction_pc, unsigned long long cycle, unsigned long long instructions,
              const std::string& status);
};
}
