#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "risc/alu/alu_options.hpp"
#include "risc/assembler/assembler.hpp"
#include "risc/exception.hpp"
#include "risc/four-stage-micro/cpu.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/control/micro_control_unit.hpp"
#include "risc/micro/datapath/datapath.hpp"
#include "risc/microassembler/four_stage_splitter.hpp"
#include "risc/microassembler/microassembler.hpp"
#include "risc/microassembler/splitter.hpp"
#include "risc/six-stage-micro/cpu.hpp"

using namespace risc;
using four_stage_micro::CpuStatus;
using four_stage_micro::InstructionPacket;
using four_stage_micro::Stage;

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

// How a processor's run ended
struct RunResult {
    bool halted = false;
    bool exception = false;
    CpuException error{};
    isa::Word exception_pc = 0;
    unsigned long long microcycles = 0;
    unsigned long long beats = 0;
    unsigned long long instructions = 0;
    unsigned long long stall_beats = 0;
    unsigned long long taken_branches = 0;
};

void print_result(const char* name, const RunResult& r, bool pipelined) {
    std::printf("\n%s:\n", name);
    if (r.halted) {
        std::printf("  Halted after %llu microcycles", r.microcycles);
    } else if (r.exception) {
        std::printf("  Exception: %s in the instruction at pc=0x%04X (after %llu microcycles",
                    EXCEPTION_NAMES[unsigned(r.error.kind)], r.exception_pc, r.microcycles);
    } else {
        std::printf("  Stopped (microcycle limit)");
    }
    if (pipelined && r.exception) {
        std::printf(", %llu beats)\n", r.beats);
    } else if (pipelined) {
        std::printf(" (%llu beats)\n", r.beats);
    } else if (r.exception) {
        std::printf(")\n");
    } else {
        std::printf("\n");
    }
    if (!pipelined) {
        return;
    }
    std::printf("  instructions finished  %llu\n", r.instructions);
    if (r.instructions > 0) {
        std::printf("  microcycles per instr  %.2f\n", double(r.microcycles) / double(r.instructions));
    }
    std::printf("  stall beats            %llu\n", r.stall_beats);
    std::printf("  taken branches         %llu\n", r.taken_branches);
}

// Usage: risc_4stage_run <program.asm> [microcode.masm] [--adder ..] [--mul ..] [--div ..]   (default microcode: microcode/vertical.masm)
// Runs the program on the four-stage pipelined microprogrammed processor, printing one line per beat,
// then runs it on the six-stage pipeline and on the sequential microprogrammed processor and compares them.
int main(int argc, char* argv[]) {
    std::string usage = std::string("Usage: risc_4stage_run <program.asm> [microcode.masm] ") + alu::ALU_OPTIONS_USAGE + "\n";
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
    four_stage_micro::CPU cpu(memory);

    memory::Memory six_memory;
    six_stage_micro::CPU six_cpu(six_memory);

    memory::Memory seq_memory;
    micro::Datapath seq_datapath;
    micro::MicroControlUnit seq_control;
    cpu.datapath.alu.configure(alu_config);
    six_cpu.datapath.alu.configure(alu_config);
    seq_datapath.alu.configure(alu_config);

    try {
        // Assemble the program and put it in the instruction region of all three memories
        assembler::Assembler assembler;
        std::vector<isa::Word> program = assembler.assemble_file(program_path);
        for (unsigned i = 0; i < program.size(); i++) {
            isa::Word address = machine::INSTRUCTION_BASE + i * isa::WORD_BYTES;
            memory.write_instruction_word(address, program[i]);
            six_memory.write_instruction_word(address, program[i]);
            seq_memory.write_instruction_word(address, program[i]);
        }

        // Microassemble the microcode: split into four and six stages for the pipelines, whole for the sequential CPU
        microassembler::Microassembler microassembler;
        microassembler::Microprogram microprogram = microassembler.assemble_file(microcode_path);
        cpu.write_stage_microprograms(microassembler::split_four(microprogram));
        six_cpu.write_stage_microprograms(microassembler::split(microprogram));
        seq_control.write_microprogram_words(microprogram.words);
        for (const microassembler::Entry& entry : microprogram.entries) {
            seq_control.switch_unit.set_start(entry.opcode, entry.address);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    // The four-stage pipeline, one beat per line
    unsigned long long beats_shown = 0;
    std::printf("beat   µcycles  %-13s %-13s %-13s %-13s\n", "IF", "OF", "EX", "MW");
    while (cpu.status == CpuStatus::Running && cpu.microcycles < MAX_MICROCYCLES) {
        // What each stage holds at the start of the beat
        InstructionPacket held[four_stage_micro::NUM_STAGES];
        for (unsigned s = unsigned(Stage::OF); s < four_stage_micro::NUM_STAGES; s++) {
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

        std::string cells[four_stage_micro::NUM_STAGES];
        cells[0] = "-";
        if (cpu.fetching) {
            cells[0] = fetch_cell(memory, fetch_pc);
        } else if (cpu.stall) {
            cells[0] = "[wait]";
        }
        for (unsigned s = unsigned(Stage::OF); s < four_stage_micro::NUM_STAGES; s++) {
            bool waited = cpu.stall && Stage(s) <= cpu.stall_stage;
            cells[s] = stage_cell(held[s], waited);
        }
        std::printf("%-6llu %-8llu", beats_shown, cpu.microcycles - start);
        for (unsigned s = 0; s < four_stage_micro::NUM_STAGES; s++) {
            std::printf(" %-13s", cells[s].c_str());
        }
        if (cpu.taken_branches != taken) {
            std::printf(" taken -> %04X", cpu.datapath.fetch.pc);
        }
        std::printf("\n");
    }

    RunResult four;
    four.halted = cpu.status == CpuStatus::Halted;
    four.exception = cpu.status == CpuStatus::Exception;
    four.error = cpu.exception;
    four.exception_pc = cpu.exception_pc;
    four.microcycles = cpu.microcycles;
    four.beats = beats_shown;
    four.instructions = cpu.instructions;
    four.stall_beats = cpu.stall_beats;
    four.taken_branches = cpu.taken_branches;
    print_result("4-stage pipelined microprogrammed processor", four, true);
    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        std::printf("  r%-2u = 0x%08X\n", r, cpu.datapath.register_file.value(r));
    }

    // The same program on the six-stage pipeline
    six_cpu.run(MAX_MICROCYCLES);
    RunResult six;
    six.halted = six_cpu.status == six_stage_micro::CpuStatus::Halted;
    six.exception = six_cpu.status == six_stage_micro::CpuStatus::Exception;
    six.error = six_cpu.exception;
    six.exception_pc = six_cpu.exception_pc;
    six.microcycles = six_cpu.microcycles;
    six.beats = six_cpu.beats;
    if (six.halted) {
        six.beats++; // the beat in which the hlt finished
    }
    six.instructions = six_cpu.instructions;
    six.stall_beats = six_cpu.stall_beats;
    six.taken_branches = six_cpu.taken_branches;
    print_result("6-stage pipelined microprogrammed processor (same program and microcode)", six, true);

    // And on the sequential microprogrammed processor
    RunResult seq;
    isa::Word seq_instruction_pc = 0;
    try {
        while (!seq_control.halted && seq.microcycles < MAX_MICROCYCLES) {
            if (seq_control.upc == 0) {
                seq_instruction_pc = seq_datapath.fetch.pc; // the preamble starts at µaddress 0
            }
            seq_control.step(seq_datapath, seq_memory);
            seq.microcycles++;
        }
    } catch (const CpuException& e) {
        seq.exception = true;
        seq.error = e;
        seq.exception_pc = seq_instruction_pc;
        seq.microcycles++;
    }
    seq.halted = seq_control.halted;
    print_result("Sequential microprogrammed processor (same program and microcode)", seq, false);

    if (four.microcycles > 0) {
        std::printf("\nMicrocycles: 4-stage %llu, 6-stage %llu, sequential %llu\n", four.microcycles,
                    six.microcycles, seq.microcycles);
        std::printf("  4-stage vs 6-stage     %.2fx\n", double(six.microcycles) / double(four.microcycles));
        std::printf("  4-stage vs sequential  %.2fx\n", double(seq.microcycles) / double(four.microcycles));
    }

    // All three processors must end in the same state
    std::string differences;
    bool same_status = (four.halted && six.halted && seq.halted) ||
                       (four.exception && six.exception && seq.exception) ||
                       (!four.halted && !four.exception && !six.halted && !six.exception && !seq.halted &&
                        !seq.exception);
    if (!same_status) {
        differences += " status";
    }
    if (four.exception && six.exception && seq.exception) {
        if (four.error.kind != seq.error.kind || four.error.kind != six.error.kind) {
            differences += " exception";
        }
        if (four.exception_pc != seq.exception_pc || four.exception_pc != six.exception_pc) {
            differences += " exception-pc";
        }
    }
    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        isa::Word value = cpu.datapath.register_file.value(r);
        if (value != seq_datapath.register_file.value(r) || value != six_cpu.datapath.register_file.value(r)) {
            differences += " r" + std::to_string(r);
        }
    }
    if (cpu.datapath.alu.flagE != seq_datapath.alu.flagE || cpu.datapath.alu.flagGT != seq_datapath.alu.flagGT ||
        cpu.datapath.alu.flagE != six_cpu.datapath.alu.flagE ||
        cpu.datapath.alu.flagGT != six_cpu.datapath.alu.flagGT) {
        differences += " flags";
    }
    for (isa::Word address = machine::DATA_BASE; address < machine::MEM_SIZE; address += isa::WORD_BYTES) {
        isa::Word value = memory.read_word(address);
        if (value != seq_memory.read_word(address) || value != six_memory.read_word(address)) {
            differences += " memory";
            break;
        }
    }

    if (differences.empty()) {
        std::printf("\nCheck: all three processors end with the same status, registers, flags and memory.\n");
    } else {
        std::printf("\nCheck FAILED, different:%s\n", differences.c_str());
        return 1;
    }
    if (cpu.status != CpuStatus::Halted) {
        return 1;
    }
    return 0;
}
