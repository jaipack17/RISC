#include "json_util.hpp"

#include <stdexcept>

#include "risc/alu/alu_options.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/micro/control/micro_decode_unit.hpp"
#include "risc/visualizer/visualizer.hpp"

namespace risc::web {

namespace {

// In the same order as micro::MicroReg
const char* REGISTER_NAMES[] = {"",        "pc",     "ir",     "I",     "rd",    "rs1",    "rs2",
                                "immx",    "branchTarget", "regSrc", "regData", "regVal", "A", "B",
                                "flagE",   "flagGT", "aluResult", "mar", "mdr",   "ldResult"};

// In the same order as micro::MicroOp
const char* MICRO_OP_NAMES[] = {"mloadIR", "mdecode", "mswitch", "mmov", "mmovi", "madd", "mbeq", "mb"};

// In the same order as micro::AluOp
const char* ALU_OP_NAMES[] = {"nop", "add", "sub", "cmp", "mul", "div", "mod",
                              "lsl", "lsr", "asr", "and", "or",  "xor", "not"};

// In the same order as micro::TransferMux and micro::MicroPCMux
const char* TRANSFER_MUX_NAMES[] = {"writeBus", "immediate", "microAdder"};
const char* MICRO_PC_MUX_NAMES[] = {"next", "branchTarget", "switch", "branchIfMBranch"};

const unsigned STACK_WORDS_BELOW_SP = 8; // free words shown under sp
const unsigned MAX_STACK_WORDS = 192;

// In the same order as isa::Exception
const char* EXCEPTION_NAMES[] = {"IllegalInstruction", "MisalignedAccess", "MisalignedFetch",
                                 "OutOfBoundsAccess", "WriteToText", "ExecuteOutsideText",
                                 "DivideByZero", "StackOverflow", "StackUnderflow"};

} // namespace

std::string register_name(micro::MicroReg reg) {
    unsigned index = unsigned(reg);
    if (index < std::size(REGISTER_NAMES)) {
        return REGISTER_NAMES[index];
    }
    return "?";
}

std::string args_name(micro::Args a) {
    if (a == micro::NO_ARGS) {
        return "";
    }
    unsigned opcode = micro::args_opcode(a);
    micro::UnitId unit = micro::args_unit(a);
    if (unit == micro::UnitId::Alu && opcode < std::size(ALU_OP_NAMES)) {
        return ALU_OP_NAMES[opcode];
    }
    if (unit == micro::UnitId::RegisterFile) {
        if (micro::RegisterFileOp(opcode) == micro::RegisterFileOp::Read) {
            return "read";
        }
        return "write";
    }
    if (unit == micro::UnitId::Memory) {
        if (micro::MemoryOp(opcode) == micro::MemoryOp::Load) {
            return "load";
        }
        return "store";
    }
    if (unit == micro::UnitId::Control) {
        return "halt";
    }
    return "?";
}

std::string args_unit_name(micro::Args a) {
    if (a == micro::NO_ARGS) {
        return "";
    }
    micro::UnitId unit = micro::args_unit(a);
    if (unit == micro::UnitId::Alu) {
        return "alu";
    }
    if (unit == micro::UnitId::RegisterFile) {
        return "registerFile";
    }
    if (unit == micro::UnitId::Memory) {
        return "memory";
    }
    return "control";
}

std::string exception_name(const CpuException& e) {
    return EXCEPTION_NAMES[unsigned(e.kind)];
}

std::string trim(const std::string& text) {
    std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return "";
    }
    std::size_t last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::string disassemble(isa::Word pc, isa::Word ir) {
    return visualizer::disassemble(pc, ir);
}

json microinstruction_json(micro::MicroWord word) {
    micro::Microinstruction m = micro::MicroDecodeUnit::fields(word);
    json j;
    j["type"] = MICRO_OP_NAMES[unsigned(m.type)];
    j["src"] = register_name(m.src);
    j["dest"] = register_name(m.dest);
    j["imm"] = m.imm;
    j["branchTarget"] = m.branchTarget;
    j["args"] = args_name(m.args);
    j["argsUnit"] = args_unit_name(m.args);
    return j;
}

json signals_json(const micro::MicroControlSignals& s) {
    json j;
    j["loadIR"] = s.loadIR;
    j["decode"] = s.decode;
    j["transfer"] = s.transfer;
    j["src"] = register_name(s.src);
    j["dest"] = register_name(s.dest);
    j["mux"] = TRANSFER_MUX_NAMES[unsigned(s.mux)];
    j["imm"] = s.imm;
    j["args"] = args_name(s.args);
    j["argsUnit"] = args_unit_name(s.args);
    j["next"] = MICRO_PC_MUX_NAMES[unsigned(s.next)];
    j["branchTarget"] = s.branchTarget;
    return j;
}

json stack_json(const memory::Memory& memory, isa::Word sp, const std::set<isa::Word>& return_slots) {
    json j;
    j["sp"] = sp;
    j["base"] = machine::STACK_BASE;
    j["end"] = machine::STACK_END;
    j["words"] = json::array();

    isa::Word aligned = sp - sp % isa::WORD_BYTES;
    if (aligned > machine::STACK_END) {
        aligned = machine::STACK_END;
    }
    isa::Word low = machine::STACK_BASE;
    if (aligned >= machine::STACK_BASE + STACK_WORDS_BELOW_SP * isa::WORD_BYTES) {
        low = aligned - STACK_WORDS_BELOW_SP * isa::WORD_BYTES;
    }
    isa::Word high = machine::STACK_END;
    if ((high - low) / isa::WORD_BYTES > MAX_STACK_WORDS) {
        high = low + MAX_STACK_WORDS * isa::WORD_BYTES;
    }
    for (isa::Word address = low; address < high; address += isa::WORD_BYTES) {
        json w;
        w["address"] = address;
        w["value"] = memory.read_word(address);
        w["ra"] = return_slots.count(address) > 0;
        j["words"].push_back(w);
    }
    return j;
}

json alu_config_json(const alu::AluConfig& config) {
    json j;
    j["adder"] = alu::adder_key(config.adder);
    j["multiplier"] = alu::multiplier_key(config.multiplier);
    j["divider"] = alu::divider_key(config.divider);
    return j;
}

alu::AluConfig alu_config_from_json(const json& j) {
    alu::AluConfig config;
    if (j.contains("adder") && !alu::read_alu_option("--adder", j["adder"].get<std::string>(), config)) {
        throw std::runtime_error("unknown adder");
    }
    if (j.contains("multiplier") && !alu::read_alu_option("--mul", j["multiplier"].get<std::string>(), config)) {
        throw std::runtime_error("unknown multiplier");
    }
    if (j.contains("divider") && !alu::read_alu_option("--div", j["divider"].get<std::string>(), config)) {
        throw std::runtime_error("unknown divider");
    }
    return config;
}
}
