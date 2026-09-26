#include "risc/visualizer/visualizer.hpp"

#include <cstdio>

namespace risc::visualizer {

namespace {

// Terminal codes
const char* CLEAR_SCREEN = "\033[2J\033[H";
const char* HIGHLIGHT = "\033[1;33m"; // bold yellow: changed this cycle
const char* ERROR = "\033[1;31m"; // bold red
const char* BOLD = "\033[1m";
const char* RESET = "\033[0m";

const unsigned MEMORY_WORDS_SHOWN = 8;

// The field of inst that starts at bit pos and is width bits wide
isa::Word field(isa::Word inst, unsigned pos, unsigned width) {
    isa::Word mask = (isa::Word{1} << width) - 1;
    return (inst >> pos) & mask;
}

// Treat the low width bits of value as a signed number
isa::SWord sign_extend(isa::Word value, unsigned width) {
    isa::Word sign_bit = isa::Word{1} << (width - 1);
    if (value & sign_bit) {
        isa::Word mask = (isa::Word{1} << width) - 1;
        value = value | ~mask;
    }
    return isa::SWord(value);
}

std::string format(const char* fmt, long long a, long long b = 0, long long c = 0) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), fmt, a, b, c);
    return buffer;
}

const char* status_name(cpu::CpuStatus status) {
    if (status == cpu::CpuStatus::Halted) {
        return "Halted";
    }
    if (status == cpu::CpuStatus::Exception) {
        return "Exception";
    }
    return "Running";
}

const char* exception_name(isa::Exception kind) {
    switch (kind) {
        case isa::Exception::IllegalInstruction:
            return "IllegalInstruction";
        case isa::Exception::MisalignedAccess:
            return "MisalignedAccess";
        case isa::Exception::MisalignedFetch:
            return "MisalignedFetch";
        case isa::Exception::OutOfBoundsAccess:
            return "OutOfBoundsAccess";
        case isa::Exception::WriteToText:
            return "WriteToText";
        case isa::Exception::ExecuteOutsideText:
            return "ExecuteOutsideText";
        case isa::Exception::DivideByZero:
            return "DivideByZero";
        case isa::Exception::StackOverflow:
            return "StackOverflow";
        case isa::Exception::StackUnderflow:
            return "StackUnderflow";
    }
    return "Unknown";
}

void append_if(std::string& text, bool on, const char* name) {
    if (on) {
        text += name;
        text += " ";
    }
}

// Names of the control signals that are 1 this cycle
std::string active_signals(const cpu::ControlSignals& c) {
    std::string text;
    append_if(text, c.isSt, "isSt");
    append_if(text, c.isLd, "isLd");
    append_if(text, c.isBeq, "isBeq");
    append_if(text, c.isBgt, "isBgt");
    append_if(text, c.isBc, "isBc");
    append_if(text, c.isBv, "isBv");
    append_if(text, c.isRet, "isRet");
    append_if(text, c.isImmediate, "isImmediate");
    append_if(text, c.isWb, "isWb");
    append_if(text, c.isUBranch, "isUBranch");
    append_if(text, c.isCall, "isCall");
    append_if(text, c.isHlt, "isHlt");
    append_if(text, c.isAdd, "isAdd");
    append_if(text, c.isSub, "isSub");
    append_if(text, c.isCmp, "isCmp");
    append_if(text, c.isMul, "isMul");
    append_if(text, c.isDiv, "isDiv");
    append_if(text, c.isMod, "isMod");
    append_if(text, c.isLsl, "isLsl");
    append_if(text, c.isLsr, "isLsr");
    append_if(text, c.isAsr, "isAsr");
    append_if(text, c.isOr, "isOr");
    append_if(text, c.isAnd, "isAnd");
    append_if(text, c.isNot, "isNot");
    append_if(text, c.isXor, "isXor");
    append_if(text, c.isMov, "isMov");
    if (text.empty()) {
        return "(none)";
    }
    return text;
}

const isa::InstructionInfo* find_info(isa::Word inst) {
    isa::Opcode opcode = isa::Opcode(field(inst, isa::OPCODE_POS, isa::OPCODE_BITS));
    for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
        if (info.opcode == opcode) {
            return &info;
        }
    }
    return nullptr;
}

std::string reg(isa::Word index) {
    return "r" + std::to_string(index);
}

// The second operand as written: a register, or the immediate (signed, or hex for u/h)
std::string second_operand(isa::Word inst) {
    bool i_bit = field(inst, isa::I_POS, 1) == 1;
    if (!i_bit) {
        return reg(field(inst, isa::RS2_POS, isa::REG_BITS));
    }
    isa::Word mod = field(inst, isa::MOD_POS, isa::MOD_BITS);
    isa::Word imm = field(inst, isa::IMM_POS, isa::IMM_BITS);
    if (mod == isa::Word(isa::Modifier::Default)) {
        return std::to_string(sign_extend(imm, isa::IMM_BITS));
    }
    return format("0x%llX", imm);
}

std::string modifier_suffix(isa::Word inst) {
    if (field(inst, isa::I_POS, 1) == 0) {
        return "";
    }
    isa::Word mod = field(inst, isa::MOD_POS, isa::MOD_BITS);
    if (mod == isa::Word(isa::Modifier::Unsigned)) {
        return "u";
    }
    if (mod == isa::Word(isa::Modifier::High)) {
        return "h";
    }
    return "";
}

void print_registers(const cpu::CPU& cpu, const std::array<isa::Word, isa::NUM_REGS>& previous, bool first_draw) {
    std::printf(" %sREGISTERS%s\n", BOLD, RESET);
    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        isa::Word value = cpu.register_value(r);
        bool changed = !first_draw && value != previous[r];

        std::string name = reg(r);
        if (r == isa::REG_SP) {
            name = "sp";
        } else if (r == isa::REG_RA) {
            name = "ra";
        }

        if (changed) {
            std::printf("   %s%-3s= 0x%08X *%s", HIGHLIGHT, name.c_str(), value, RESET);
        } else {
            std::printf("   %-3s= 0x%08X  ", name.c_str(), value);
        }
        if (r % 4 == 3) {
            std::printf("\n");
        }
    }
}

void print_flags(const cpu::CPU& cpu, std::uint8_t previous, bool first_draw) {
    const char* names[] = {"E", "GT", "C", "V"};
    const std::uint8_t bits[] = {isa::FLAG_E, isa::FLAG_GT, isa::FLAG_C, isa::FLAG_V};

    std::printf(" %sFLAGS%s     ", BOLD, RESET);
    for (unsigned i = 0; i < 4; i++) {
        bool now = cpu.flags().is_set(bits[i]);
        bool before = (previous & bits[i]) != 0;
        if (!first_draw && now != before) {
            std::printf("%s%s=%d *%s   ", HIGHLIGHT, names[i], now, RESET);
        } else {
            std::printf("%s=%d     ", names[i], now);
        }
    }
    std::printf("\n");
}

void print_units(const cpu::CycleTrace& t) {
    const cpu::ControlSignals& c = t.ctrl;

    std::printf(" IF   pc=0x%04X -> instruction memory -> inst=0x%08X\n", t.pc, t.inst);

    std::printf(" OF   read port 1: A=%-3s D=0x%08X   read port 2: A=%-3s D=0x%08X\n",
                reg(t.read_port1.address).c_str(), t.read_port1.data,
                reg(t.read_port2.address).c_str(), t.read_port2.data);
    std::printf("      out: op1=0x%08X op2=0x%08X immx=0x%08X branchTarget=0x%04X\n",
                t.of.op1, t.of.op2, t.of.immx, t.of.branchTarget);

    const char* b_source = "op2";
    isa::Word b = t.of.op2;
    if (c.isImmediate) {
        b_source = "immx";
        b = t.of.immx;
    }
    std::printf(" EX   ALU: A=op1=0x%08X B=%s=0x%08X -> aluResult=0x%08X\n", t.of.op1, b_source, b, t.ex.aluResult);
    if (t.ex.isBranchTaken) {
        std::printf("      branch unit: %sTAKEN -> branchPC=0x%04X%s\n", HIGHLIGHT, t.ex.branchPC, RESET);
    } else {
        std::printf("      branch unit: not taken\n");
    }

    if (c.isLd) {
        std::printf(" MA   %sREAD  mem[mar=0x%04X] -> mdr=0x%08X%s\n", HIGHLIGHT, t.mar, t.mdr, RESET);
    } else if (c.isSt) {
        std::printf(" MA   %sWRITE mem[mar=0x%04X] <- mdr=0x%08X%s\n", HIGHLIGHT, t.mar, t.mdr, RESET);
    } else {
        std::printf(" MA   idle (no ld/st)\n");
    }

    if (t.write_port.enable) {
        std::printf(" RW   write port: E=1 A=%s D=0x%08X  ->  %s%s <- 0x%08X%s\n",
                    reg(t.write_port.address).c_str(), t.write_port.data,
                    HIGHLIGHT, reg(t.write_port.address).c_str(), t.write_port.data, RESET);
    } else {
        std::printf(" RW   write port: E=0 (no register written)\n");
    }

    std::printf(" PC   0x%04X -> 0x%04X\n", t.pc, t.next_pc);
}

}

std::string disassemble(isa::Word pc, isa::Word inst) {
    const isa::InstructionInfo* info = find_info(inst);
    if (info == nullptr) {
        return format(".word 0x%08llX", inst);
    }

    std::string name(info->mnemonic);
    std::string rd = reg(field(inst, isa::RD_POS, isa::REG_BITS));
    std::string rs1 = reg(field(inst, isa::RS1_POS, isa::REG_BITS));

    switch (info->operands) {
        case isa::Operands::RdRs1Src2:
            return name + modifier_suffix(inst) + " " + rd + ", " + rs1 + ", " + second_operand(inst);
        case isa::Operands::Rs1Src2:
            return name + modifier_suffix(inst) + " " + rs1 + ", " + second_operand(inst);
        case isa::Operands::RdSrc2:
            return name + modifier_suffix(inst) + " " + rd + ", " + second_operand(inst);
        case isa::Operands::RdMem:
            return name + " " + rd + ", " + second_operand(inst) + "[" + rs1 + "]";
        case isa::Operands::Label: {
            isa::SWord offset = sign_extend(field(inst, isa::OFFSET_POS, isa::OFFSET_BITS), isa::OFFSET_BITS);
            isa::Word target = pc + (isa::Word(offset) << 2);
            return name + " " + format("%+lld  (-> 0x%04llX)", offset, target);
        }
        case isa::Operands::None:
            return name;
    }
    return name;
}

void Visualizer::draw(const cpu::CPU& cpu, const memory::Memory& memory) {
    const cpu::CycleTrace& t = cpu.last;

    std::printf("%s", CLEAR_SCREEN);
    std::printf("%s═══ RISC201 ═══ cycle %llu ═══ instructions %llu ═══ status: %s ═══%s\n",
                BOLD, (unsigned long long)cpu.cycles, (unsigned long long)cpu.instructions,
                status_name(cpu.status), RESET);

    if (cpu.cycles == 0) {
        std::printf(" reset state, nothing executed yet (next pc = 0x%04X)\n", cpu.pc());
    } else {
        std::printf(" %sINSTRUCTION%s  pc=0x%04X  inst=0x%08X  %s%s%s\n",
                    BOLD, RESET, t.pc, t.inst, HIGHLIGHT, disassemble(t.pc, t.inst).c_str(), RESET);
        std::printf(" %sCONTROL%s      %s\n", BOLD, RESET, active_signals(t.ctrl).c_str());
        std::printf("───────────────────────────────────────────────────────────────────────────\n");
        print_units(t);
    }

    if (cpu.status == cpu::CpuStatus::Exception) {
        std::printf(" %sEXCEPTION: %s at pc=0x%04X (address 0x%04X)%s\n",
                    ERROR, exception_name(cpu.exception.kind), cpu.exception_pc, cpu.exception.address, RESET);
    }

    std::printf("───────────────────────────────────────────────────────────────────────────\n");
    print_registers(cpu, previous_registers, first_draw);
    print_flags(cpu, previous_flags, first_draw);

    // Memory view: move the window to the word accessed this cycle, if it is outside the view
    bool accessed = cpu.cycles > 0 && (t.ctrl.isLd || t.ctrl.isSt) && t.mar < machine::MEM_SIZE;
    if (accessed) {
        isa::Word window_end = memory_window + MEMORY_WORDS_SHOWN * isa::WORD_BYTES;
        if (t.mar < memory_window || t.mar >= window_end) {
            memory_window = t.mar - t.mar % (MEMORY_WORDS_SHOWN * isa::WORD_BYTES);
        }
    }

    std::printf(" %sMEMORY%s   ", BOLD, RESET);
    for (unsigned i = 0; i < MEMORY_WORDS_SHOWN; i++) {
        isa::Word address = memory_window + i * isa::WORD_BYTES;
        isa::Word value = memory.read_word(address);
        if (accessed && address == t.mar) {
            std::printf("%s[0x%04X]=%u *%s  ", HIGHLIGHT, address, value, RESET);
        } else {
            std::printf("[0x%04X]=%u  ", address, value);
        }
        if (i == MEMORY_WORDS_SHOWN / 2 - 1) {
            std::printf("\n          ");
        }
    }
    std::printf("\n");
    std::printf("───────────────────────────────────────────────────────────────────────────\n");
    std::printf(" * = changed or accessed this cycle\n");
    std::fflush(stdout);

    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        previous_registers[r] = cpu.register_value(r);
    }
    previous_flags = cpu.flags().bits;
    first_draw = false;
}
}
