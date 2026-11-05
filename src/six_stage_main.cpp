#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "risc/alu/alu_options.hpp"
#include "risc/assembler/assembler.hpp"
#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/control/micro_control_unit.hpp"
#include "risc/micro/datapath/datapath.hpp"
#include "risc/microassembler/microassembler.hpp"
#include "risc/microassembler/splitter.hpp"
#include "risc/six-stage-micro/cpu.hpp"

using namespace risc;
using six_stage_micro::CpuStatus;
using six_stage_micro::InstructionPacket;
using six_stage_micro::Stage;

const char* STAGE_NAMES[] = {"IF", "OF1", "OF2", "EX", "MA", "RW"};

// In the same order as isa::Exception
const char* EXCEPTION_NAMES[] = {"IllegalInstruction", "MisalignedAccess", "MisalignedFetch",
                                 "OutOfBoundsAccess", "WriteToText", "ExecuteOutsideText",
                                 "DivideByZero", "StackOverflow", "StackUnderflow"};

const unsigned long long MAX_MICROCYCLES = 1000000;

// The mnemonic of an instruction word, e.g. "add" ("?" for an unused opcode)
std::string mnemonic_of(isa::Word ir) {
    unsigned opcode = (ir >> isa::OPCODE_POS) & ((1u << isa::OPCODE_BITS) - 1);
    for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
        if (unsigned(info.opcode) == opcode) {
            return std::string(info.mnemonic);
        }
    }
    return "?";
}

// One column of the beat trace: the instruction's address and mnemonic, "-" for a bubble,
// in brackets if it waited this beat
std::string stage_cell(const InstructionPacket& p, bool waited) {
    if (p.bubble) {
        return "-";
    }
    char text[32];
    std::snprintf(text, sizeof(text), "%04X %s", p.currentPc, mnemonic_of(p.ir).c_str());
    if (waited) {
        return "[" + std::string(text) + "]";
    }
    return text;
}

// IF's column: the address it fetched from this beat
std::string fetch_cell(const memory::Memory& memory, isa::Word pc) {
    char text[32];
    if (pc % isa::WORD_BYTES == 0 && pc >= machine::INSTRUCTION_BASE && pc < machine::INSTRUCTION_END) {
        std::snprintf(text, sizeof(text), "%04X %s", pc, mnemonic_of(memory.read_word(pc)).c_str());
    } else {
        std::snprintf(text, sizeof(text), "%04X", pc);
    }
    return text;
}

// Usage: risc_6stage_run <program.asm> [microcode.masm] [--adder ..] [--mul ..] [--div ..]   (default microcode: microcode/vertical.masm)
// Runs the program on the six-stage pipelined microprogrammed processor, printing one line per beat,
// then runs it on the sequential microprogrammed processor and compares the two.
int main(int argc, char* argv[]) {
    std::string usage = std::string("Usage: risc_6stage_run <program.asm> [microcode.masm] ") + alu::ALU_OPTIONS_USAGE + "\n";
    std::string program_path;
    std::string microcode_path = "microcode/vertical.masm";
    alu::AluConfig alu_config;
    bool has_microcode = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.rfind("--", 0) == 0) {
            if (i + 1 >= argc || !alu::read_alu_option(arg, argv[i + 1], alu_config)) {
                std::cerr << usage;
                return 1;
            }
            i++;
        } else if (program_path.empty()) {
            program_path = arg;
        } else if (!has_microcode) {
            microcode_path = arg;
            has_microcode = true;
        } else {
            std::cerr << usage;
            return 1;
        }
    }
    if (program_path.empty()) {
        std::cerr << usage;
        return 1;
    }

    memory::Memory memory;
    six_stage_micro::CPU cpu(memory);

    memory::Memory seq_memory;
    micro::Datapath seq_datapath;
    micro::MicroControlUnit seq_control;
    cpu.datapath.alu.configure(alu_config);
    seq_datapath.alu.configure(alu_config);

    try {
        // Assemble the program and put it in the instruction region of both memories
        assembler::Assembler assembler;
        std::vector<isa::Word> program = assembler.assemble_file(program_path);
        for (unsigned i = 0; i < program.size(); i++) {
            memory.write_instruction_word(machine::INSTRUCTION_BASE + i * isa::WORD_BYTES, program[i]);
            seq_memory.write_instruction_word(machine::INSTRUCTION_BASE + i * isa::WORD_BYTES, program[i]);
        }

        // Microassemble the microcode: split into the six stages for the pipeline, whole for the sequential CPU
        microassembler::Microassembler microassembler;
        microassembler::Microprogram microprogram = microassembler.assemble_file(microcode_path);
        cpu.write_stage_microprograms(microassembler::split(microprogram));
        seq_control.write_microprogram_words(microprogram.words);
        for (const microassembler::Entry& entry : microprogram.entries) {
            seq_control.switch_unit.set_start(entry.opcode, entry.address);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    // The pipeline, one beat per line
    unsigned long long beats_shown = 0;
    std::printf("beat   µcycles  %-13s %-13s %-13s %-13s %-13s %-13s\n", "IF", "OF1", "OF2", "EX", "MA", "RW");
    while (cpu.status == CpuStatus::Running && cpu.microcycles < MAX_MICROCYCLES) {
        // What each stage holds at the start of the beat
        InstructionPacket held[six_stage_micro::NUM_STAGES];
        for (unsigned s = unsigned(Stage::OF1); s < six_stage_micro::NUM_STAGES; s++) {
            held[s] = cpu.datapath.packet_of(Stage(s));
        }
        isa::Word fetch_pc = cpu.datapath.fetch.pc;
        unsigned long long beat = cpu.beats;
        unsigned long long start = cpu.microcycles;
        unsigned long long taken = cpu.taken_branches;

        while (cpu.status == CpuStatus::Running && cpu.beats == beat && cpu.microcycles < MAX_MICROCYCLES) {
            cpu.step();
        }
        if (cpu.microcycles == start) {
            break; // the exception was reported before the beat started
        }
        beats_shown++;

        std::string cells[six_stage_micro::NUM_STAGES];
        cells[0] = "-";
        if (cpu.fetching) {
            cells[0] = fetch_cell(memory, fetch_pc);
        } else if (cpu.stall) {
            cells[0] = "[wait]";
        }
        for (unsigned s = unsigned(Stage::OF1); s < six_stage_micro::NUM_STAGES; s++) {
            bool waited = cpu.stall && Stage(s) <= cpu.stall_stage;
            cells[s] = stage_cell(held[s], waited);
        }
        std::printf("%-6llu %-8llu", beats_shown, cpu.microcycles - start);
        for (unsigned s = 0; s < six_stage_micro::NUM_STAGES; s++) {
            std::printf(" %-13s", cells[s].c_str());
        }
        if (cpu.taken_branches != taken) {
            std::printf(" taken -> %04X", cpu.datapath.fetch.pc);
        }
        std::printf("\n");
    }

    std::printf("\n6-stage pipelined microprogrammed processor:\n");
    if (cpu.status == CpuStatus::Halted) {
        std::printf("  Halted after %llu microcycles (%llu beats)\n", (unsigned long long)cpu.microcycles,
                    beats_shown);
    } else if (cpu.status == CpuStatus::Exception) {
        std::printf("  Exception: %s in the instruction at pc=0x%04X (after %llu microcycles, %llu beats)\n",
                    EXCEPTION_NAMES[unsigned(cpu.exception.kind)], cpu.exception_pc,
                    (unsigned long long)cpu.microcycles, beats_shown);
    } else {
        std::printf("  Stopped (microcycle limit)\n");
    }
    std::printf("  instructions finished  %llu\n", (unsigned long long)cpu.instructions);
    if (cpu.instructions > 0) {
        std::printf("  microcycles per instr  %.2f\n", double(cpu.microcycles) / double(cpu.instructions));
    }
    std::printf("  stall beats            %llu\n", (unsigned long long)cpu.stall_beats);
    std::printf("  taken branches         %llu\n", (unsigned long long)cpu.taken_branches);
    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        std::printf("  r%-2u = 0x%08X\n", r, cpu.datapath.register_file.value(r));
    }

    // The same program on the sequential microprogrammed processor
    unsigned long long seq_cycles = 0;
    isa::Word seq_instruction_pc = 0;
    bool seq_exception = false;
    CpuException seq_error{};
    try {
        while (!seq_control.halted && seq_cycles < MAX_MICROCYCLES) {
            if (seq_control.upc == 0) {
                seq_instruction_pc = seq_datapath.fetch.pc; // the preamble starts at µaddress 0
            }
            seq_control.step(seq_datapath, seq_memory);
            seq_cycles++;
        }
    } catch (const CpuException& e) {
        seq_exception = true;
        seq_error = e;
        seq_cycles++;
    }

    std::printf("\nSequential microprogrammed processor (same program and microcode):\n");
    if (seq_control.halted) {
        std::printf("  Halted after %llu microcycles\n", seq_cycles);
    } else if (seq_exception) {
        std::printf("  Exception: %s in the instruction at pc=0x%04X (after %llu microcycles)\n",
                    EXCEPTION_NAMES[unsigned(seq_error.kind)], seq_instruction_pc, seq_cycles);
    } else {
        std::printf("  Stopped (microcycle limit)\n");
    }
    if (cpu.microcycles > 0) {
        std::printf("  speedup of the pipeline  %.2fx\n", double(seq_cycles) / double(cpu.microcycles));
    }

    // Both processors must end in the same state
    std::string differences;
    bool same_status = (cpu.status == CpuStatus::Halted && seq_control.halted) ||
                       (cpu.status == CpuStatus::Exception && seq_exception) ||
                       (cpu.status == CpuStatus::Running && !seq_control.halted && !seq_exception);
    if (!same_status) {
        differences += " status";
    }
    if (cpu.status == CpuStatus::Exception && seq_exception) {
        if (cpu.exception.kind != seq_error.kind) {
            differences += " exception";
        }
        if (cpu.exception_pc != seq_instruction_pc) {
            differences += " exception-pc";
        }
    }
    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        if (cpu.datapath.register_file.value(r) != seq_datapath.register_file.value(r)) {
            differences += " r" + std::to_string(r);
        }
    }
    if (cpu.datapath.alu.flagE != seq_datapath.alu.flagE || cpu.datapath.alu.flagGT != seq_datapath.alu.flagGT) {
        differences += " flags";
    }
    for (isa::Word address = machine::DATA_BASE; address < machine::MEM_SIZE; address += isa::WORD_BYTES) {
        if (memory.read_word(address) != seq_memory.read_word(address)) {
            differences += " memory";
            break;
        }
    }

    if (differences.empty()) {
        std::printf("\nCheck: both processors end with the same status, registers, flags and memory.\n");
    } else {
        std::printf("\nCheck FAILED, different:%s\n", differences.c_str());
        return 1;
    }
    if (cpu.status != CpuStatus::Halted) {
        return 1;
    }
    return 0;
}
