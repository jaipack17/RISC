#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "risc/assembler/assembler.hpp"
#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/control/micro_control_unit.hpp"
#include "risc/micro/datapath/datapath.hpp"
#include "risc/microassembler/microassembler.hpp"
#include "risc/visualizer/micro_visualizer.hpp"

using namespace risc;

// In the same order as isa::Exception
const char* EXCEPTION_NAMES[] = {"IllegalInstruction", "MisalignedAccess", "MisalignedFetch",
                                 "OutOfBoundsAccess", "WriteToText", "ExecuteOutsideText",
                                 "DivideByZero", "StackOverflow", "StackUnderflow"};

// Usage: risc_micro_view <program.asm>
// Press Enter to run the next microcycle, or type q and Enter to quit.
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: risc_micro_view <program.asm>\n";
        return 1;
    }

    memory::Memory memory;
    micro::Datapath datapath;
    micro::MicroControlUnit control;
    std::vector<isa::Word> program;
    microassembler::Microprogram microprogram;

    try {
        assembler::Assembler assembler;
        program = assembler.assemble_file(argv[1]);
        for (unsigned i = 0; i < program.size(); i++) {
            memory.write_instruction_word(machine::INSTRUCTION_BASE + i * isa::WORD_BYTES, program[i]);
        }

        microassembler::Microassembler microassembler;
        microprogram = microassembler.assemble_file("microcode/vertical.masm");
        control.write_microprogram_words(microprogram.words);
        for (const microassembler::Entry& entry : microprogram.entries) {
            control.switch_unit.set_start(entry.opcode, entry.address);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    visualizer::MicroVisualizer view(program, microprogram.entries);
    unsigned long long cycle = 0;
    unsigned long long instructions = 0;
    isa::Word instruction_pc = 0;

    while (!control.halted && cycle < 100000) {
        unsigned upc = control.upc;
        if (upc == 0) {
            instruction_pc = datapath.fetch.pc; // the preamble starts a new instruction
            instructions++;
        }

        std::string status = "Running";
        try {
            control.step(datapath, memory);
        } catch (const CpuException& e) {
            status = std::string("Exception: ") + EXCEPTION_NAMES[unsigned(e.kind)];
            cycle++;
            view.draw(datapath, control, upc, instruction_pc, cycle, instructions, status);
            return 1;
        }
        cycle++;
        if (control.halted) {
            status = "Halted";
        }

        view.draw(datapath, control, upc, instruction_pc, cycle, instructions, status);

        // Manual clock: one microcycle per Enter
        if (!control.halted) {
            std::printf("Enter = next cycle, q = quit > ");
            std::fflush(stdout);
            std::string input;
            if (!std::getline(std::cin, input) || input == "q") {
                return 0;
            }
        }
    }
    return 0;
}
