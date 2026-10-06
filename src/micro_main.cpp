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
#include "risc/micro/control/micro_decode_unit.hpp"
#include "risc/micro/datapath/datapath.hpp"
#include "risc/microassembler/microassembler.hpp"

using namespace risc;
using namespace risc::micro;

const char* MICRO_OP_NAMES[] = {"mloadIR", "mdecode", "mswitch", "mmov", "mmovi", "madd", "mbeq", "mb"};

// In the same order as isa::Exception
const char* EXCEPTION_NAMES[] = {"IllegalInstruction", "MisalignedAccess", "MisalignedFetch",
                                 "OutOfBoundsAccess", "WriteToText", "ExecuteOutsideText",
                                 "DivideByZero", "StackOverflow", "StackUnderflow"};

// Usage: risc_micro_run <program.asm> [microcode.masm]   (default microcode: microcode/vertical.masm)
int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: risc_micro_run <program.asm> [microcode.masm]\n";
        return 1;
    }
    std::string program_path = argv[1];
    std::string microcode_path = "microcode/vertical.masm";
    if (argc == 3) {
        microcode_path = argv[2];
    }

    memory::Memory memory;
    Datapath datapath;
    MicroControlUnit control;

    try {
        // Assemble the program and put it in the instruction region
        assembler::Assembler assembler;
        std::vector<isa::Word> program = assembler.assemble_file(program_path);
        for (unsigned i = 0; i < program.size(); i++) {
            memory.write_instruction_word(machine::INSTRUCTION_BASE + i * isa::WORD_BYTES, program[i]);
        }

        // Microassemble the microcode and put it in the µcontrol unit
        microassembler::Microassembler microassembler;
        microassembler::Microprogram microprogram = microassembler.assemble_file(microcode_path);
        control.write_microprogram_words(microprogram.words);
        for (const microassembler::Entry& entry : microprogram.entries) {
            control.switch_unit.set_start(entry.opcode, entry.address);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    // Run one microinstruction per cycle until <halt>, an exception, or the cycle limit
    unsigned long long cycle = 0;
    isa::Word instruction_pc = 0; // pc of the instruction being executed
    try {
        while (!control.halted && cycle < 100000) {
            unsigned upc = control.upc;
            if (upc == 0) {
                instruction_pc = datapath.fetch.pc; // the preamble starts at µaddress 0
            }
            Microinstruction m = MicroDecodeUnit::fields(control.microprogram[upc]);

            control.step(datapath, memory);
            cycle++;

            std::printf("cycle %-5llu µpc=%-4u %-8s -> µpc=%-4u pc=0x%04X\n",
                        cycle, upc, MICRO_OP_NAMES[unsigned(m.type)], control.upc, datapath.fetch.pc);
        }
    } catch (const CpuException& e) {
        std::printf("\nException: %s in the instruction at pc=0x%04X (cycle %llu)\n",
                    EXCEPTION_NAMES[unsigned(e.kind)], instruction_pc, cycle + 1);
        return 1;
    }

    std::printf("\n%s after %llu cycles, pc=0x%04X\n", control.halted ? "Halted" : "Stopped (cycle limit)",
                cycle, datapath.fetch.pc);
    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        std::printf("  r%-2u = 0x%08X\n", r, datapath.register_file.value(r));
    }
    return 0;
}
