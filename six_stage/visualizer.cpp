#include "risc/cpu/six_stage/visualizer.hpp"

#include <cstdio>
#include <string>

namespace risc::cpu::six_stage {

namespace {
constexpr const char* CLEAR = "\033[2J\033[H";
constexpr const char* BOLD = "\033[1m";
constexpr const char* YELLOW = "\033[1;33m";
constexpr const char* RED = "\033[1;31m";
constexpr const char* RESET = "\033[0m";
constexpr unsigned MEMORY_WORDS = 8;

const char* micro_op_name(micro::MicroOp op) {
    static constexpr const char* names[] = {
        "mloadIR", "mdecode", "mswitch", "mmov",
        "mmovi", "madd", "mbeq", "mb"
    };
    return names[static_cast<unsigned>(op)];
}

const char* status_name(Status status) {
    switch (status) {
    case Status::Running: return "Running";
    case Status::Halted: return "Halted";
    case Status::Exception: return "Exception";
    }
    return "Unknown";
}

const char* exception_name(isa::Exception kind) {
    switch (kind) {
    case isa::Exception::IllegalInstruction: return "IllegalInstruction";
    case isa::Exception::MisalignedAccess: return "MisalignedAccess";
    case isa::Exception::MisalignedFetch: return "MisalignedFetch";
    case isa::Exception::OutOfBoundsAccess: return "OutOfBoundsAccess";
    case isa::Exception::WriteToText: return "WriteToText";
    case isa::Exception::ExecuteOutsideText: return "ExecuteOutsideText";
    case isa::Exception::DivideByZero: return "DivideByZero";
    case isa::Exception::StackOverflow: return "StackOverflow";
    case isa::Exception::StackUnderflow: return "StackUnderflow";
    }
    return "Unknown";
}

std::string mnemonic(isa::Word instruction) {
    const auto opcode = static_cast<isa::Opcode>(
        (instruction >> isa::OPCODE_POS) & ((1u << isa::OPCODE_BITS) - 1));
    for (const auto& info : isa::INSTRUCTIONS) {
        if (info.opcode == opcode) return std::string(info.mnemonic);
    }
    return ".word";
}

bool memory_access(const micro::Microinstruction& instruction) {
    return instruction.args != micro::NO_ARGS &&
           micro::args_unit(instruction.args) == micro::UnitId::Memory;
}

void print_registers(const CPU& cpu,
                     const std::array<isa::Word, isa::NUM_REGS>& previous,
                     bool first_draw) {
    std::printf(" %sREGISTERS%s\n", BOLD, RESET);
    for (unsigned r = 0; r < isa::NUM_REGS; ++r) {
        const isa::Word value = cpu.register_value(r);
        const bool changed = !first_draw && value != previous[r];
        const char* name = r == isa::REG_SP ? "sp" : r == isa::REG_RA ? "ra" : nullptr;
        const std::string generic_name = "r" + std::to_string(r);
        if (name == nullptr) name = generic_name.c_str();
        std::printf("   %s%-3s = 0x%08X%s%s", changed ? YELLOW : "", name,
                    value, changed ? " *" : "", changed ? RESET : "  ");
        if (r % 4 == 3) std::printf("\n");
    }
}

void print_flags(const micro::ALU& alu, std::uint8_t previous, bool first_draw) {
    const char* names[] = {"E", "GT", "C", "V"};
    const bool values[] = {alu.flagE, alu.flagGT, alu.flagC, alu.flagV};
    const std::uint8_t bits[] = {isa::FLAG_E, isa::FLAG_GT, isa::FLAG_C, isa::FLAG_V};
    std::printf(" %sFLAGS%s     ", BOLD, RESET);
    for (unsigned i = 0; i < 4; ++i) {
        const bool changed = !first_draw && values[i] != ((previous & bits[i]) != 0);
        std::printf("%s%s=%d%s%s   ", changed ? YELLOW : "", names[i], values[i],
                    changed ? " *" : "", changed ? RESET : "");
    }
    std::printf("\n");
}

} // namespace

void Visualizer::draw(const CPU& cpu, const memory::Memory& memory) {
    const auto& trace = cpu.last;
    const auto& datapath = cpu.micro_datapath();
    const auto instruction = micro::MicroDecodeUnit::fields(trace.microinstruction);
    const bool has_cycle = cpu.cycles != 0;

    std::printf("%s", CLEAR);
    std::printf("%s=== RISC201 SIX-STAGE MICROCODED CPU === cycle %llu === retired %llu === %s%s\n",
                BOLD, static_cast<unsigned long long>(cpu.cycles),
                static_cast<unsigned long long>(cpu.instructions_retired),
                status_name(cpu.status), RESET);

    if (!has_cycle) {
        std::printf(" Reset state: next instruction PC=0x%04X, microPC=%u\n",
                    cpu.pc(), cpu.micro_pc());
    } else {
        std::printf(" %sINSTRUCTION%s  pc=0x%04X  inst=0x%08X  %s\n",
                    BOLD, RESET, trace.instruction_pc, datapath.fetch.ir,
                    mnemonic(datapath.fetch.ir).c_str());
        std::printf(" %sMICROCODE%s    phase=%-6s upc=%-3u %-8s -> upc=%-3u  pc=0x%04X\n",
                    BOLD, RESET, stage_name(trace.stage), trace.micro_pc,
                    micro_op_name(trace.micro_op), trace.next_micro_pc, cpu.pc());
        std::printf("                src=%u dest=%u imm=%d branchTarget=u%u args=0x%03X\n",
                    unsigned(instruction.src), unsigned(instruction.dest),
                    instruction.imm, instruction.branchTarget,
                    unsigned(instruction.args));

        std::printf("------------------------------------------------------------------------\n");
        std::printf(" IF   pc=0x%04X  ir=0x%08X  (%s)\n", datapath.fetch.pc,
                    datapath.fetch.ir, mnemonic(datapath.fetch.ir).c_str());
        std::printf(" OF   I=%d rd=%u rs1=%u rs2=%u immx=0x%08X target=0x%04X\n",
                    datapath.decode.I, datapath.decode.rd, datapath.decode.rs1,
                    datapath.decode.rs2, datapath.decode.immx,
                    datapath.decode.branchTarget);
        std::printf("      regSrc=%u regData=0x%08X regVal=0x%08X\n",
                    datapath.register_file.regSrc, datapath.register_file.regData,
                    datapath.register_file.regVal);
        std::printf(" ALU  A=0x%08X B=0x%08X result=0x%08X\n",
                    datapath.alu.A, datapath.alu.B, datapath.alu.aluResult);
        std::printf(" BR   flags E=%d GT=%d C=%d V=%d; branch/micro-sequencer phase=%s\n",
                    datapath.alu.flagE, datapath.alu.flagGT,
                    datapath.alu.flagC, datapath.alu.flagV,
                    trace.stage == Stage::Branch ? "active" : "idle");
        std::printf(" MA   mar=0x%04X mdr=0x%08X ldResult=0x%08X%s\n",
                    datapath.memory_unit.mar, datapath.memory_unit.mdr,
                    datapath.memory_unit.ldResult,
                    memory_access(instruction) ? "  * memory operation" : "");
        std::printf(" RW   destination=r%u data=0x%08X%s\n",
                    datapath.register_file.regSrc, datapath.register_file.regData,
                    instruction.args != micro::NO_ARGS &&
                            micro::args_unit(instruction.args) == micro::UnitId::RegisterFile &&
                            micro::args_opcode(instruction.args) ==
                                unsigned(micro::RegisterFileOp::Write)
                        ? "  * write requested" : "");
    }

    if (cpu.status == Status::Exception) {
        std::printf(" %sEXCEPTION: %s at instruction PC=0x%04X (address=0x%04X)%s\n",
                    RED, exception_name(cpu.exception.kind), cpu.exception_pc,
                    cpu.exception.address, RESET);
    }

    std::printf("------------------------------------------------------------------------\n");
    print_registers(cpu, previous_registers, first_draw);
    print_flags(datapath.alu, previous_flags, first_draw);

    const bool accessed = has_cycle && memory_access(instruction) &&
                          datapath.memory_unit.mar < machine::MEM_SIZE;
    if (accessed) {
        const isa::Word end = memory_window + MEMORY_WORDS * isa::WORD_BYTES;
        if (datapath.memory_unit.mar < memory_window || datapath.memory_unit.mar >= end) {
            memory_window = datapath.memory_unit.mar -
                datapath.memory_unit.mar % (MEMORY_WORDS * isa::WORD_BYTES);
        }
    }
    std::printf(" %sMEMORY%s   ", BOLD, RESET);
    for (unsigned i = 0; i < MEMORY_WORDS; ++i) {
        const isa::Word address = memory_window + i * isa::WORD_BYTES;
        const bool is_access = accessed && address == datapath.memory_unit.mar;
        std::printf("%s[0x%04X]=0x%08X%s%s  ", is_access ? YELLOW : "", address,
                    memory.read_word(address), is_access ? " *" : "",
                    is_access ? RESET : "");
        if (i == MEMORY_WORDS / 2 - 1) std::printf("\n          ");
    }
    std::printf("\n------------------------------------------------------------------------\n");
    std::printf(" * = changed or accessed this microinstruction\n");
    std::fflush(stdout);

    for (unsigned r = 0; r < isa::NUM_REGS; ++r) {
        previous_registers[r] = cpu.register_value(r);
    }
    previous_flags = (datapath.alu.flagE ? isa::FLAG_E : 0) |
                     (datapath.alu.flagGT ? isa::FLAG_GT : 0) |
                     (datapath.alu.flagC ? isa::FLAG_C : 0) |
                     (datapath.alu.flagV ? isa::FLAG_V : 0);
    first_draw = false;
}

} // namespace risc::cpu::six_stage
