#include "risc/visualizer/micro_visualizer.hpp"

#include <cstdio>
#include <iterator>

#include "risc/micro/control/micro_decode_unit.hpp"
#include "risc/visualizer/visualizer.hpp"

namespace risc::visualizer {

namespace {

using micro::MicroReg;

// ANSI terminal codes
const char* CLEAR_SCREEN = "\033[2J\033[H";
const char* HIGHLIGHT = "\033[1;33m"; // bold yellow: current line, or changed this cycle
const char* BOLD = "\033[1m";
const char* DIM = "\033[2m";
const char* RESET = "\033[0m";

const unsigned PANEL_ROWS = 12; // rows shown in the program and microcode panels
const unsigned LEFT_WIDTH = 44; // width of the program panel

const char* MICRO_OP_NAMES[] = {"mloadIR", "mdecode", "mswitch", "mmov", "mmovi", "madd", "mbeq", "mb"};

// In MicroReg order
const char* REGISTER_NAMES[] = {"-", "pc", "ir", "I", "rd", "rs1", "rs2", "immx", "branchTarget",
                                "regSrc", "regData", "regVal", "A", "B", "flagE", "flagGT",
                                "aluResult", "mar", "mdr", "ldResult"};

// In AluOp, RegisterFileOp, MemoryOp and ControlOp order
const char* ALU_OP_NAMES[] = {"nop", "add", "sub", "cmp", "mul", "div", "mod",
                              "lsl", "lsr", "asr", "and", "or", "xor", "not"};
const char* REGISTER_FILE_OP_NAMES[] = {"nop", "read", "write"};
const char* MEMORY_OP_NAMES[] = {"nop", "load", "store"};
const char* CONTROL_OP_NAMES[] = {"nop", "halt"};

const char* MUX_NAMES[] = {"write bus", "µimm", "µadder"};

// One line of a panel, and whether to draw it highlighted, dimmed or plain
struct Row {
    std::string text;
    const char* color = "";
};

std::string hex(isa::Word value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%08X", value);
    return buffer;
}

std::string address_text(isa::Word address) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%04X", address);
    return buffer;
}

// "<read>", "<add>", ... or "" for no args
std::string args_name(micro::Args a) {
    if (a == micro::NO_ARGS) {
        return "";
    }
    unsigned opcode = micro::args_opcode(a);
    micro::UnitId unit = micro::args_unit(a);
    const char* name = "?";
    if (unit == micro::UnitId::Alu && opcode < std::size(ALU_OP_NAMES)) {
        name = ALU_OP_NAMES[opcode];
    } else if (unit == micro::UnitId::RegisterFile && opcode < std::size(REGISTER_FILE_OP_NAMES)) {
        name = REGISTER_FILE_OP_NAMES[opcode];
    } else if (unit == micro::UnitId::Memory && opcode < std::size(MEMORY_OP_NAMES)) {
        name = MEMORY_OP_NAMES[opcode];
    } else if (unit == micro::UnitId::Control && opcode < std::size(CONTROL_OP_NAMES)) {
        name = CONTROL_OP_NAMES[opcode];
    }
    return std::string("<") + name + ">";
}

const char* reg_name(MicroReg reg) {
    return REGISTER_NAMES[unsigned(reg)];
}

// A microinstruction as text, e.g. "mmov    regSrc, rs1, <read>"
std::string micro_text(const micro::Microinstruction& m) {
    std::string name = MICRO_OP_NAMES[unsigned(m.type)];
    std::string padded = name;
    padded.resize(8, ' ');
    std::string a = args_name(m.args);
    if (!a.empty()) {
        a = ", " + a;
    }

    if (m.type == micro::MicroOp::mmov) {
        return padded + reg_name(m.dest) + ", " + reg_name(m.src) + a;
    }
    if (m.type == micro::MicroOp::mmovi || m.type == micro::MicroOp::madd) {
        return padded + reg_name(m.dest) + ", " + std::to_string(m.imm) + a;
    }
    if (m.type == micro::MicroOp::mbeq) {
        return padded + reg_name(m.src) + ", " + std::to_string(m.imm) + ", µ" + std::to_string(m.branchTarget);
    }
    if (m.type == micro::MicroOp::mb) {
        return padded + "µ" + std::to_string(m.branchTarget);
    }
    return name;
}

// The value of a microcode-visible register (Table 9.7), for display
isa::Word internal_value(const micro::Datapath& d, MicroReg reg) {
    switch (reg) {
        case MicroReg::pc:
            return d.fetch.pc;
        case MicroReg::ir:
            return d.fetch.ir;
        case MicroReg::I:
            return d.decode.I;
        case MicroReg::rd:
            return d.decode.rd;
        case MicroReg::rs1:
            return d.decode.rs1;
        case MicroReg::rs2:
            return d.decode.rs2;
        case MicroReg::immx:
            return d.decode.immx;
        case MicroReg::branchTarget:
            return d.decode.branchTarget;
        case MicroReg::regSrc:
            return d.register_file.regSrc;
        case MicroReg::regData:
            return d.register_file.regData;
        case MicroReg::regVal:
            return d.register_file.regVal;
        case MicroReg::A:
            return d.alu.A;
        case MicroReg::B:
            return d.alu.B;
        case MicroReg::flagE:
            return d.alu.flagE;
        case MicroReg::flagGT:
            return d.alu.flagGT;
        case MicroReg::aluResult:
            return d.alu.aluResult;
        case MicroReg::mar:
            return d.memory_unit.mar;
        case MicroReg::mdr:
            return d.memory_unit.mdr;
        case MicroReg::ldResult:
            return d.memory_unit.ldResult;
        case MicroReg::None:
            return 0;
    }
    return 0;
}

std::string mnemonic_of(isa::Opcode opcode) {
    for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
        if (info.opcode == opcode) {
            return std::string(info.mnemonic);
        }
    }
    return "?";
}

// Prints `text` padded to `width` visible characters (µ, ► and ✓ count as one each)
void print_padded(const Row& row, unsigned width) {
    unsigned visible = 0;
    for (unsigned char c : row.text) {
        if ((c & 0xC0) != 0x80) {
            visible++; // count the first byte of each UTF-8 character only
        }
    }
    std::printf("%s%s%s", row.color, row.text.c_str(), RESET);
    for (unsigned i = visible; i < width; i++) {
        std::printf(" ");
    }
}

}

MicroVisualizer::MicroVisualizer(const std::vector<isa::Word>& program,
                                 const std::vector<microassembler::Entry>& entries)
    : program(program), entries(entries) {}

void MicroVisualizer::draw(const micro::Datapath& d, const micro::MicroControlUnit& control, unsigned executed_upc,
                           isa::Word instruction_pc, unsigned long long cycle, unsigned long long instructions,
                           const std::string& status) {
    if (executed_upc == 0) {
        executed.fill(false); // the preamble starts a new instruction
    }
    executed[executed_upc] = true;

    // The routine the µpc is in: the entry with the largest address <= executed_upc, or the preamble
    unsigned routine_start = 0;
    std::string routine_name = "preamble";
    for (const microassembler::Entry& entry : entries) {
        if (entry.address <= executed_upc && entry.address >= routine_start) {
            routine_start = entry.address;
            routine_name = mnemonic_of(entry.opcode);
        }
    }
    unsigned routine_end = micro::MICROPROGRAM_SIZE;
    for (const microassembler::Entry& entry : entries) {
        if (entry.address > routine_start && entry.address < routine_end) {
            routine_end = entry.address;
        }
    }
    if (routine_end > routine_start + PANEL_ROWS) {
        routine_end = routine_start + PANEL_ROWS;
    }

    std::printf("%s", CLEAR_SCREEN);
    std::printf("%s══ RISC201 microprogrammed ══ cycle %llu ══ instruction %llu ══ µpc %u ══ %s ══%s\n",
                BOLD, cycle, instructions, executed_upc, status.c_str(), RESET);

    // ---- Program panel (left) and microcode panel (right) ----
    std::vector<Row> left;
    left.push_back(Row{"── PROGRAM ──", BOLD});
    unsigned current = instruction_pc / isa::WORD_BYTES;
    unsigned first = 0;
    if (current > PANEL_ROWS / 2) {
        first = current - PANEL_ROWS / 2;
    }
    for (unsigned i = first; i < program.size() && i < first + PANEL_ROWS; i++) {
        isa::Word address = i * isa::WORD_BYTES;
        std::string line = address_text(address) + "  " + disassemble(address, program[i]);
        if (address == instruction_pc) {
            left.push_back(Row{" ► " + line, HIGHLIGHT});
        } else {
            left.push_back(Row{"   " + line, ""});
        }
    }

    std::vector<Row> right;
    right.push_back(Row{"── MICROCODE: " + routine_name + " ──", BOLD});
    for (unsigned u = routine_start; u < routine_end; u++) {
        micro::Microinstruction m = micro::MicroDecodeUnit::fields(control.microprogram[u]);
        std::string line = "µ" + std::to_string(u) + "  " + micro_text(m);
        if (u == executed_upc) {
            right.push_back(Row{" ► " + line, HIGHLIGHT});
        } else if (executed[u]) {
            right.push_back(Row{" ✓ " + line, ""});
        } else {
            right.push_back(Row{"   " + line, DIM});
        }
    }

    for (unsigned r = 0; r < left.size() || r < right.size(); r++) {
        Row empty;
        print_padded(r < left.size() ? left[r] : empty, LEFT_WIDTH);
        std::printf(" │ ");
        print_padded(r < right.size() ? right[r] : empty, 0);
        std::printf("\n");
    }

    // ---- µcontrol unit ----
    const micro::MicroControlSignals& s = control.signals;
    micro::Microinstruction m = micro::MicroDecodeUnit::fields(control.microprogram[executed_upc]);
    std::printf("──────────────────────────────────────────────────────────────────────────────────────────\n");
    std::printf(" %sµCONTROL%s    µ%u  %s%s%s   (word 0x%012llX)\n", BOLD, RESET, executed_upc, HIGHLIGHT,
                micro_text(m).c_str(), RESET, (unsigned long long)control.microprogram[executed_upc]);

    if (s.next == micro::MicroPCMux::Switch) {
        isa::Opcode opcode = isa::Opcode(d.fetch.ir >> isa::OPCODE_POS);
        std::printf("             µmux: switch unit, opcode '%s' -> µ%u\n", mnemonic_of(opcode).c_str(), control.upc);
    } else if (s.next == micro::MicroPCMux::BranchTarget) {
        std::printf("             µmux: branch -> µ%u\n", control.upc);
    } else if (s.next == micro::MicroPCMux::BranchIfMBranch) {
        if (d.isMBranch) {
            std::printf("             µmux: isMBranch = 1, branch taken -> µ%u\n", control.upc);
        } else {
            std::printf("             µmux: isMBranch = 0, not taken -> µ%u\n", control.upc);
        }
    } else {
        std::printf("             µmux: next -> µ%u\n", control.upc);
    }

    // ---- What happened in the datapath ----
    std::printf(" %sDATAPATH%s    ", BOLD, RESET);
    if (s.loadIR) {
        std::printf("fetch unit: ir <- memory[pc %s] = %s\n", address_text(d.fetch.pc).c_str(), hex(d.fetch.ir).c_str());
    } else if (s.decode) {
        std::printf("decode unit: I=%u rd=r%u rs1=r%u rs2=r%u immx=%s branchTarget=%s\n", unsigned(d.decode.I),
                    d.decode.rd, d.decode.rs1, d.decode.rs2, hex(d.decode.immx).c_str(),
                    address_text(d.decode.branchTarget).c_str());
    } else if (s.transfer) {
        std::string source = "(nothing)";
        if (s.src != MicroReg::None) {
            source = reg_name(s.src);
        }
        std::string destination = "(no destination)";
        if (s.dest != MicroReg::None && s.next != micro::MicroPCMux::BranchIfMBranch) {
            destination = reg_name(s.dest);
        }
        std::printf("bus: %s = %s on the write bus, transfer mux picks %s, read bus = %s -> %s\n",
                    source.c_str(), hex(d.write_bus).c_str(), MUX_NAMES[unsigned(s.mux)],
                    hex(d.read_bus).c_str(), destination.c_str());
    } else {
        std::printf("(no datapath action, only the µpc changes)\n");
    }

    if (s.args != micro::NO_ARGS) {
        micro::UnitId unit = micro::args_unit(s.args);
        std::printf("             args %s -> ", args_name(s.args).c_str());
        if (unit == micro::UnitId::Alu) {
            if (micro::AluOp(micro::args_opcode(s.args)) == micro::AluOp::Cmp) {
                std::printf("ALU: flagE = %d, flagGT = %d\n", d.alu.flagE, d.alu.flagGT);
            } else {
                std::printf("ALU: aluResult = %s\n", hex(d.alu.aluResult).c_str());
            }
        } else if (unit == micro::UnitId::RegisterFile) {
            if (micro::RegisterFileOp(micro::args_opcode(s.args)) == micro::RegisterFileOp::Read) {
                std::printf("register file: regVal = r%u = %s\n", d.register_file.regSrc, hex(d.register_file.regVal).c_str());
            } else {
                std::printf("register file: r%u = regData = %s\n", d.register_file.regSrc, hex(d.register_file.regData).c_str());
            }
        } else if (unit == micro::UnitId::Memory) {
            if (micro::MemoryOp(micro::args_opcode(s.args)) == micro::MemoryOp::Load) {
                std::printf("memory unit: ldResult = memory[%s] = %s\n", hex(d.memory_unit.mar).c_str(), hex(d.memory_unit.ldResult).c_str());
            } else {
                std::printf("memory unit: memory[%s] = mdr = %s\n", hex(d.memory_unit.mar).c_str(), hex(d.memory_unit.mdr).c_str());
            }
        } else {
            std::printf("µcontrol unit: halt\n");
        }
    }

    // ---- Internal registers (Table 9.7), changed ones highlighted ----
    std::printf("──────────────────────────────────────────────────────────────────────────────────────────\n");
    std::printf(" %sINTERNAL REGISTERS%s\n", BOLD, RESET);
    const MicroReg groups[5][6] = {
        {MicroReg::pc, MicroReg::ir},
        {MicroReg::I, MicroReg::rd, MicroReg::rs1, MicroReg::rs2, MicroReg::immx, MicroReg::branchTarget},
        {MicroReg::regSrc, MicroReg::regData, MicroReg::regVal},
        {MicroReg::A, MicroReg::B, MicroReg::aluResult, MicroReg::flagE, MicroReg::flagGT},
        {MicroReg::mar, MicroReg::mdr, MicroReg::ldResult},
    };
    const char* group_names[5] = {"fetch", "decode", "register file", "ALU", "memory unit"};
    for (unsigned g = 0; g < 5; g++) {
        std::printf("   %-14s", group_names[g]);
        for (MicroReg reg : groups[g]) {
            if (reg == MicroReg::None) {
                continue;
            }
            isa::Word value = internal_value(d, reg);
            bool changed = !first_draw && value != previous_internal[unsigned(reg)];
            std::string text = std::string(reg_name(reg)) + "=" + hex(value);
            if (reg == MicroReg::I || reg == MicroReg::rd || reg == MicroReg::rs1 || reg == MicroReg::rs2 ||
                reg == MicroReg::regSrc || reg == MicroReg::flagE || reg == MicroReg::flagGT) {
                text = std::string(reg_name(reg)) + "=" + std::to_string(value);
            }
            if (changed) {
                std::printf("%s%s*%s  ", HIGHLIGHT, text.c_str(), RESET);
            } else {
                std::printf("%s   ", text.c_str());
            }
        }
        std::printf("\n");
    }

    // ---- External registers r0-r15 ----
    std::printf(" %sEXTERNAL REGISTERS%s\n", BOLD, RESET);
    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        isa::Word value = d.register_file.value(r);
        bool changed = !first_draw && value != previous_registers[r];
        std::string name = "r" + std::to_string(r);
        if (r == isa::REG_SP) {
            name = "sp";
        } else if (r == isa::REG_RA) {
            name = "ra";
        }
        if (changed) {
            std::printf("   %s%-3s= %s *%s", HIGHLIGHT, name.c_str(), hex(value).c_str(), RESET);
        } else {
            std::printf("   %-3s= %s  ", name.c_str(), hex(value).c_str());
        }
        if (r % 4 == 3) {
            std::printf("\n");
        }
    }
    std::printf(" * = changed this cycle     ✓ = microinstruction already run for this instruction\n");
    std::fflush(stdout);

    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        previous_registers[r] = d.register_file.value(r);
    }
    for (unsigned r = 0; r < previous_internal.size(); r++) {
        previous_internal[r] = internal_value(d, MicroReg(r));
    }
    first_draw = false;
}
}
