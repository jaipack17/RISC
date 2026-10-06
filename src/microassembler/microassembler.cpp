#include "risc/microassembler/microassembler.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>

#include "risc/micro/micro_isa.hpp"

namespace risc::microassembler {

namespace {

// One non-empty source line, split into tokens
struct Line {
    unsigned number = 0;
    std::vector<std::string> tokens;
};

// Split one line into lowercase tokens. Spaces and commas separate tokens,
// ':' is kept as its own token (it ends a label), and '@' starts a comment.
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;

    for (char c : text) {
        if (c == '@') {
            break;
        }
        if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ':') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            if (c == ':') {
                tokens.push_back(":");
            }
        } else {
            current += char(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

// Tokenize every line, keeping its line number and skipping blank or comment-only lines
std::vector<Line> read_lines(const std::string& source) {
    std::vector<Line> lines;
    std::istringstream input(source);
    std::string text;
    unsigned number = 0;

    while (std::getline(input, text)) {
        number++;
        std::vector<std::string> tokens = tokenize(text);
        if (!tokens.empty()) {
            lines.push_back(Line{number, tokens});
        }
    }
    return lines;
}

// A microinstruction after pass 1: its tokens (label removed) and its µaddress
struct ParsedLine {
    unsigned number = 0;
    std::vector<std::string> tokens;
    unsigned address = 0;
};

// A label name: '.', then a letter or '_', then letters, digits or '_'
bool is_label_name(const std::string& name) {
    if (name.size() < 2 || name[0] != '.') {
        return false;
    }
    if (!std::isalpha(static_cast<unsigned char>(name[1])) && name[1] != '_') {
        return false;
    }
    for (unsigned i = 2; i < name.size(); i++) {
        if (!std::isalnum(static_cast<unsigned char>(name[i])) && name[i] != '_') {
            return false;
        }
    }
    return true;
}

// The instruction whose mnemonic is `name`, or nullptr if there is none
const isa::InstructionInfo* find_mnemonic(const std::string& name) {
    for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
        if (info.mnemonic == name) {
            return &info;
        }
    }
    return nullptr;
}

// Pass 1: give every microinstruction its µaddress, record every label,
// and add every entry label (.add, .ld, ...) to the switch table
std::vector<ParsedLine> first_pass(const std::vector<Line>& lines,
                                   std::map<std::string, unsigned>& labels,
                                   std::vector<Entry>& entries) {
    std::vector<ParsedLine> code;
    unsigned address = 0;

    for (const Line& line : lines) {
        std::vector<std::string> tokens = line.tokens;

        // A label definition: ".name" followed by ":"
        if (tokens.size() >= 2 && tokens[1] == ":") {
            const std::string& name = tokens[0];
            if (!is_label_name(name)) {
                throw MicroassemblerError(line.number, "invalid label '" + name + "'");
            }
            if (labels.count(name) != 0) {
                throw MicroassemblerError(line.number, "label '" + name + "' is defined twice");
            }
            labels[name] = address;

            const isa::InstructionInfo* info = find_mnemonic(name.substr(1));
            if (info != nullptr) {
                entries.push_back(Entry{info->opcode, address});
            }

            tokens.erase(tokens.begin(), tokens.begin() + 2);
        }

        // Whatever is left on the line is a microinstruction
        if (!tokens.empty()) {
            code.push_back(ParsedLine{line.number, tokens, address});
            address++;
        }
    }
    return code;
}

// Table 9.7 register names, lowercase, in MicroReg order (index 0 is MicroReg::None)
const char* REGISTER_NAMES[] = {"", "pc", "ir", "i", "rd", "rs1", "rs2", "immx", "branchtarget",
                                "regsrc", "regdata", "regval", "a", "b", "flage", "flaggt",
                                "aluresult", "mar", "mdr", "ldresult"};

micro::MicroReg parse_register(unsigned line, const std::string& token) {
    for (unsigned r = 1; r < std::size(REGISTER_NAMES); r++) {
        if (token == REGISTER_NAMES[r]) {
            if (micro::MicroReg(r) == micro::MicroReg::ir) {
                throw MicroassemblerError(line, "'ir' is not connected to the bus");
            }
            return micro::MicroReg(r);
        }
    }
    throw MicroassemblerError(line, "unknown register '" + token + "'");
}

// A register that loads from the read bus (§9.7: not the decode registers or the flags)
micro::MicroReg parse_destination(unsigned line, const std::string& token) {
    micro::MicroReg reg = parse_register(line, token);
    bool connected = reg == micro::MicroReg::pc || reg == micro::MicroReg::regSrc ||
                     reg == micro::MicroReg::regData || reg == micro::MicroReg::regVal ||
                     reg == micro::MicroReg::A || reg == micro::MicroReg::B ||
                     reg == micro::MicroReg::aluResult || reg == micro::MicroReg::mar ||
                     reg == micro::MicroReg::mdr || reg == micro::MicroReg::ldResult;
    if (!connected) {
        throw MicroassemblerError(line, "'" + token + "' cannot be a destination (it only drives the bus)");
    }
    return reg;
}

// Decimal or 0x hexadecimal, in the 12-bit signed range
int parse_immediate(unsigned line, const std::string& token) {
    long long value = 0;
    std::size_t used = 0;
    try {
        value = std::stoll(token, &used, 0);
    } catch (const std::exception&) {
        throw MicroassemblerError(line, "invalid immediate '" + token + "'");
    }
    if (used != token.size()) {
        throw MicroassemblerError(line, "invalid immediate '" + token + "'");
    }
    if (value < -2048 || value > 2047) {
        throw MicroassemblerError(line, "immediate " + token + " is outside -2048..2047");
    }
    return int(value);
}

// <args> name -> the args value for that unit and operation
micro::Args parse_args(unsigned line, const std::string& token) {
    using micro::AluOp;
    using micro::args;

    if (token == "<add>") return args(AluOp::Add);
    if (token == "<sub>") return args(AluOp::Sub);
    if (token == "<cmp>") return args(AluOp::Cmp);
    if (token == "<mul>") return args(AluOp::Mul);
    if (token == "<div>") return args(AluOp::Div);
    if (token == "<mod>") return args(AluOp::Mod);
    if (token == "<lsl>") return args(AluOp::Lsl);
    if (token == "<lsr>") return args(AluOp::Lsr);
    if (token == "<asr>") return args(AluOp::Asr);
    if (token == "<and>") return args(AluOp::And);
    if (token == "<or>") return args(AluOp::Or);
    if (token == "<xor>") return args(AluOp::Xor);
    if (token == "<not>") return args(AluOp::Not);
    if (token == "<read>") return args(micro::RegisterFileOp::Read);
    if (token == "<write>") return args(micro::RegisterFileOp::Write);
    if (token == "<load>") return args(micro::MemoryOp::Load);
    if (token == "<store>") return args(micro::MemoryOp::Store);
    if (token == "<halt>") return args(micro::ControlOp::Halt);
    throw MicroassemblerError(line, "unknown args '" + token + "'");
}

// A label used as an operand (mbeq, mb) -> its µaddress from pass 1
unsigned parse_label(unsigned line, const std::string& token, const std::map<std::string, unsigned>& labels) {
    if (!is_label_name(token)) {
        throw MicroassemblerError(line, "expected a label, got '" + token + "'");
    }
    if (labels.count(token) == 0) {
        throw MicroassemblerError(line, "undefined label '" + token + "'");
    }
    return labels.at(token);
}

// Throws unless the microinstruction has between `min` and `max` operands
void check_operand_count(const ParsedLine& p, unsigned min, unsigned max, const std::string& form) {
    unsigned count = p.tokens.size() - 1;
    if (count < min || count > max) {
        throw MicroassemblerError(p.number, "'" + p.tokens[0] + "' expects " + form);
    }
}

// Pass 2: turn one microinstruction's tokens into its fields (Table 9.8)
micro::Microinstruction parse_microinstruction(const ParsedLine& p, const std::map<std::string, unsigned>& labels) {
    const std::vector<std::string>& t = p.tokens;
    const std::string& op = t[0];
    unsigned line = p.number;

    micro::Args a = micro::NO_ARGS;
    if (t.size() == 4 && (op == "mmov" || op == "mmovi" || op == "madd")) {
        a = parse_args(line, t[3]);
    }

    if (op == "mloadir") {
        check_operand_count(p, 0, 0, "no operands");
        return micro::mloadIR();
    }
    if (op == "mdecode") {
        check_operand_count(p, 0, 0, "no operands");
        return micro::mdecode();
    }
    if (op == "mswitch") {
        check_operand_count(p, 0, 0, "no operands");
        return micro::mswitch();
    }
    if (op == "mmov") {
        check_operand_count(p, 2, 3, "dest, src [, <args>]");
        return micro::mmov(parse_destination(line, t[1]), parse_register(line, t[2]), a);
    }
    if (op == "mmovi") {
        check_operand_count(p, 2, 3, "dest, imm [, <args>]");
        return micro::mmovi(parse_destination(line, t[1]), parse_immediate(line, t[2]), a);
    }
    if (op == "madd") {
        check_operand_count(p, 2, 3, "reg, imm [, <args>]");
        return micro::madd(parse_destination(line, t[1]), parse_immediate(line, t[2]), a);
    }
    if (op == "mbeq") {
        check_operand_count(p, 3, 3, "reg, imm, .label");
        return micro::mbeq(parse_register(line, t[1]), parse_immediate(line, t[2]), parse_label(line, t[3], labels));
    }
    if (op == "mb") {
        check_operand_count(p, 1, 1, ".label");
        return micro::mb(parse_label(line, t[1], labels));
    }
    throw MicroassemblerError(line, "unknown microinstruction '" + op + "'");
}

}

Microprogram Microassembler::assemble_file(const std::string& path) const {
    std::ifstream file(path);
    if (!file) {
        throw MicroassemblerError(0, "cannot open " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return assemble(buffer.str());
}

Microprogram Microassembler::assemble(const std::string& source) const {
    Microprogram program;
    std::map<std::string, unsigned> labels;

    // Pass 1: µaddresses, labels, switch table
    std::vector<ParsedLine> code = first_pass(read_lines(source), labels, program.entries);

    if (code.size() > micro::MICROPROGRAM_SIZE) {
        throw MicroassemblerError(0, "microprogram has " + std::to_string(code.size()) +
                                         " microinstructions, the limit is " +
                                         std::to_string(micro::MICROPROGRAM_SIZE));
    }

    // Pass 2: each microinstruction -> its 45-bit word
    for (const ParsedLine& p : code) {
        program.words.push_back(micro::encode(parse_microinstruction(p, labels)));
    }

    // Warn about instructions that have no microcode
    for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
        bool found = false;
        for (const Entry& entry : program.entries) {
            if (entry.opcode == info.opcode) {
                found = true;
            }
        }
        if (!found) {
            program.warnings.push_back("no microcode for '" + std::string(info.mnemonic) + "'");
        }
    }
    return program;
}
}
