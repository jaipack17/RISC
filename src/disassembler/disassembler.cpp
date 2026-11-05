#include "risc/disassembler/disassembler.hpp"

#include <cstdio>
#include <fstream>
#include <map>
#include <stdexcept>

namespace risc::disassembler {

namespace {

// The field of word that starts at bit pos and is width bits wide
isa::Word field(isa::Word word, unsigned pos, unsigned width) {
    isa::Word mask = (isa::Word{1} << width) - 1;
    return (word >> pos) & mask;
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

std::string hex(isa::Word value, int digits) {
    char text[16];
    std::snprintf(text, sizeof(text), "%0*X", digits, value);
    return text;
}

const isa::InstructionInfo* find_info(isa::Word word) {
    isa::Opcode opcode = isa::Opcode(field(word, isa::OPCODE_POS, isa::OPCODE_BITS));
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

bool has_immediate(isa::Word word) {
    return field(word, isa::I_POS, 1) == 1;
}

// The second operand as written: a register, or the immediate (signed, or hex for u/h)
std::string second_operand(isa::Word word) {
    if (!has_immediate(word)) {
        return reg(field(word, isa::RS2_POS, isa::REG_BITS));
    }
    isa::Word modifier = field(word, isa::MOD_POS, isa::MOD_BITS);
    isa::Word imm = field(word, isa::IMM_POS, isa::IMM_BITS);
    if (modifier == isa::Word(isa::Modifier::Default)) {
        return std::to_string(sign_extend(imm, isa::IMM_BITS));
    }
    return "0x" + hex(imm, 1);
}

std::string modifier_suffix(isa::Word word) {
    if (!has_immediate(word)) {
        return "";
    }
    isa::Word modifier = field(word, isa::MOD_POS, isa::MOD_BITS);
    if (modifier == isa::Word(isa::Modifier::Unsigned)) {
        return "u";
    }
    if (modifier == isa::Word(isa::Modifier::High)) {
        return "h";
    }
    return "";
}

bool is_branch(isa::Word word) {
    const isa::InstructionInfo* info = find_info(word);
    return info != nullptr && info->operands == isa::Operands::Label;
}

bool is_call(isa::Word word) {
    return isa::Opcode(field(word, isa::OPCODE_POS, isa::OPCODE_BITS)) == isa::Opcode::CALL;
}

// Why the word cannot be written as assembly ("" if it can)
std::string problem_of(isa::Word word) {
    const isa::InstructionInfo* info = find_info(word);
    if (info == nullptr) {
        return "unused opcode " + std::to_string(field(word, isa::OPCODE_POS, isa::OPCODE_BITS));
    }
    // Branches use those bits for the offset, and nop, hlt and ret ignore them
    bool has_operand_fields = info->operands != isa::Operands::Label && info->operands != isa::Operands::None;
    if (has_operand_fields && has_immediate(word) && field(word, isa::MOD_POS, isa::MOD_BITS) == 0b11) {
        return "immediate modifier 0b11 (an illegal instruction)";
    }
    return "";
}
} // namespace

std::string instruction_text(isa::Word word, const std::string& branch_operand) {
    const isa::InstructionInfo* info = find_info(word);
    if (info == nullptr) {
        return "";
    }

    std::string name(info->mnemonic);
    std::string rd = reg(field(word, isa::RD_POS, isa::REG_BITS));
    std::string rs1 = reg(field(word, isa::RS1_POS, isa::REG_BITS));

    switch (info->operands) {
        case isa::Operands::RdRs1Src2:
            return name + modifier_suffix(word) + " " + rd + ", " + rs1 + ", " + second_operand(word);
        case isa::Operands::Rs1Src2:
            return name + modifier_suffix(word) + " " + rs1 + ", " + second_operand(word);
        case isa::Operands::RdSrc2:
            return name + modifier_suffix(word) + " " + rd + ", " + second_operand(word);
        case isa::Operands::RdMem:
            return name + " " + rd + ", " + second_operand(word) + "[" + rs1 + "]";
        case isa::Operands::Label:
            return name + " " + branch_operand;
        case isa::Operands::None:
            return name;
    }
    return name;
}

isa::Word branch_target(isa::Word address, isa::Word word) {
    isa::SWord offset = sign_extend(field(word, isa::OFFSET_POS, isa::OFFSET_BITS), isa::OFFSET_BITS);
    return address + (isa::Word(offset) << 2);
}

std::vector<DisassembledLine> Disassembler::disassemble(const std::vector<isa::Word>& words, isa::Word base) const {
    isa::Word end = base + isa::Word(words.size()) * isa::WORD_BYTES;

    // Pass 1: a label for every branch and call target inside the program (or just after it)
    std::map<isa::Word, std::string> labels;
    for (unsigned i = 0; i < words.size(); i++) {
        isa::Word address = base + i * isa::WORD_BYTES;
        if (!is_branch(words[i])) {
            continue;
        }
        isa::Word target = branch_target(address, words[i]);
        if (target < base || target > end) {
            continue;
        }
        std::string name = "L_" + hex(target, 4);
        if (is_call(words[i])) {
            name = "f_" + hex(target, 4);
        }
        if (labels.count(target) == 0 || is_call(words[i])) {
            labels[target] = name; // a call target keeps f_ even if a branch also goes there
        }
    }

    // Pass 2: one line per word
    std::vector<DisassembledLine> lines;
    for (unsigned i = 0; i < words.size(); i++) {
        DisassembledLine line;
        line.address = base + i * isa::WORD_BYTES;
        line.word = words[i];
        if (labels.count(line.address) > 0) {
            line.label = labels[line.address];
        }
        line.problem = problem_of(words[i]);
        if (line.problem.empty() && is_branch(words[i])) {
            isa::Word target = branch_target(line.address, words[i]);
            if (labels.count(target) > 0) {
                line.text = instruction_text(words[i], labels[target]);
            } else {
                line.problem = "branch to 0x" + hex(target, 4) + ", outside the program";
            }
        } else if (line.problem.empty()) {
            line.text = instruction_text(words[i], "");
        }
        lines.push_back(line);
    }

    // A label just after the last word (a branch to the end of the program) goes on a line of its own
    if (labels.count(end) > 0) {
        DisassembledLine line;
        line.address = end;
        line.label = labels[end];
        lines.push_back(line);
    }
    return lines;
}

std::string Disassembler::source(const std::vector<DisassembledLine>& lines, const std::string& title) const {
    std::string text = "@ " + title + "\n";
    for (const DisassembledLine& line : lines) {
        bool is_word = !line.text.empty() || !line.problem.empty();
        if (!line.label.empty()) {
            if (line.address != lines.front().address) {
                text += "\n";
            }
            text += line.label + ":\n";
        }
        if (!is_word) {
            continue;
        }
        std::string comment = "@ " + hex(line.address, 4) + "  " + hex(line.word, 8);
        if (line.problem.empty()) {
            std::string code = "    " + line.text;
            if (code.size() < 28) {
                code += std::string(28 - code.size(), ' ');
            }
            text += code + comment + "\n";
        } else {
            text += "    " + comment + "  cannot disassemble: " + line.problem + "\n";
        }
    }
    return text;
}

std::vector<isa::Word> Disassembler::read_hex_file(const std::string& path) const {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(path + ": cannot open");
    }
    std::vector<isa::Word> words;
    std::string line;
    unsigned number = 0;
    while (std::getline(input, line)) {
        number++;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        bool valid = line.size() == 8;
        for (char c : line) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                valid = false;
            }
        }
        if (!valid) {
            throw std::runtime_error(path + ":" + std::to_string(number) + ": expected an 8-digit hexadecimal word, got '" + line + "'");
        }
        words.push_back(isa::Word(std::stoul(line, nullptr, 16)));
    }
    return words;
}
}
