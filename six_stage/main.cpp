#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>

#include "risc/assembler/assembler.hpp"
#include "risc/cpu/six_stage/cpu.hpp"
#include "risc/cpu/six_stage/visualizer.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/microassembler/microassembler.hpp"

#ifndef RISC201_DEFAULT_MICROCODE_PATH
#define RISC201_DEFAULT_MICROCODE_PATH "microcode/vertical.masm"
#endif

namespace {
}

int main(int argc, char* argv[]) {
    using namespace risc;

    if (argc > 4) {
        std::cerr << "Usage: risc_six_stage [program.asm] [microcode.masm] [clock-ms]\n";
        return 1;
    }

    unsigned clock_ms = 100;
    if (argc >= 4) {
        try {
            const unsigned long parsed = std::stoul(argv[3]);
            if (parsed > 60000) throw std::out_of_range("clock period");
            clock_ms = static_cast<unsigned>(parsed);
        } catch (const std::exception&) {
            std::cerr << "clock-ms must be an integer from 0 to 60000\n";
            return 1;
        }
    }

    // Default demo: MOV r1, 40; MOV r2, 2; ADD r3, r1, r2; HLT.
    constexpr std::array<isa::Word, 4> demo_program = {
        0x84400028,
        0x84800002,
        0x00C48000,
        0xA0000000,
    };

    memory::Memory memory;

    try {
        std::vector<isa::Word> program;
        if (argc >= 2) {
            assembler::Assembler program_assembler;
            program = program_assembler.assemble_file(argv[1]);
        } else {
            program.assign(demo_program.begin(), demo_program.end());
        }
        for (unsigned i = 0; i < program.size(); ++i) {
            memory.write_instruction_word(machine::INSTRUCTION_BASE + i * isa::WORD_BYTES,
                                          program[i]);
        }

        microassembler::Microassembler assembler;
        const microassembler::Microprogram microcode =
            assembler.assemble_file(argc >= 3 ? argv[2]
                                              : RISC201_DEFAULT_MICROCODE_PATH);
        cpu::six_stage::CPU cpu(memory, microcode);
        cpu::six_stage::Visualizer visualizer;

        visualizer.draw(cpu, memory);

        constexpr std::uint64_t max_cycles = 100000;
        while (cpu.status == cpu::six_stage::Status::Running &&
               cpu.cycles < max_cycles) {
            if (clock_ms != 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(clock_ms));
            }
            cpu.step();
            visualizer.draw(cpu, memory);
        }

        std::cout << "status=" << (cpu.status == cpu::six_stage::Status::Halted
                                        ? "halted"
                                        : cpu.status == cpu::six_stage::Status::Exception
                                              ? "exception"
                                              : "cycle limit")
                  << " cycles=" << cpu.cycles
                  << " retired=" << cpu.instructions_retired
                  << " r3=" << cpu.register_value(3) << '\n';
        for (unsigned reg = 0; reg < isa::NUM_REGS; ++reg) {
            std::printf("r%-2u = 0x%08X\n", reg, cpu.register_value(reg));
        }

        return cpu.status == cpu::six_stage::Status::Halted ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Microcode error: " << error.what() << '\n';
        return 1;
    }
}
