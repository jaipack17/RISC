#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "risc/assembler/assembler.hpp"
#include "risc/cpu/cpu.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/visualizer/visualizer.hpp"

using namespace risc;

// Usage: risc_run <program.asm>
// Press Enter to run the next cycle, or type q and Enter to quit.
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: risc_run <program.asm>\n";
        return 1;
    }

    memory::Memory memory;

    // Assemble the program and put it in the instruction region
    try {
        assembler::Assembler assembler;
        std::vector<isa::Word> program = assembler.assemble_file(argv[1]);
        for (unsigned i = 0; i < program.size(); i++) {
            memory.write_instruction_word(machine::INSTRUCTION_BASE + i * isa::WORD_BYTES, program[i]);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    cpu::CPU cpu(memory);
    visualizer::Visualizer visualizer;

    // Manual clock: one cycle per Enter
    visualizer.draw(cpu, memory);
    while (cpu.status == cpu::CpuStatus::Running) {
        std::printf("Enter = next cycle, q = quit > ");
        std::fflush(stdout);
        std::string input;
        if (!std::getline(std::cin, input) || input == "q") {
            return 0;
        }
        cpu.step();
        visualizer.draw(cpu, memory);
    }
}
