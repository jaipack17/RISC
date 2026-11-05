#include "risc/assembler/assembler.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace risc::assembler {

namespace {

using isa::Word;

//number: line number and text: instruction
struct SourceLine {
    unsigned number = 0;
    std::string text;
    std::string macro; // the macro line this instruction was expanded from, e.g. "push r1" ("" if none)
};

enum class OperandKind { Register, Immediate, Memory, Label };

struct Operand {
    OperandKind kind;
    int reg = 0;
    std::int64_t immediate = 0;
    std::string label;
};

struct Instruction {
    const isa::InstructionInfo* info = nullptr;
    isa::Modifier modifier = isa::Modifier::Default;
    std::vector<Operand> operands;
};

struct ParsedLine {
    SourceLine source;
    std::string label;
    Instruction instruction;
    bool has_instruction = false;
    Word address = 0;
};

//map to store address of labels
using SymbolTable = std::unordered_map<std::string, Word>;

//lowercase conversion
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return char(std::tolower(ch));
    });
    return value;
}

//trimming whitespaces
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

//check whether something is legal label
bool is_identifier(std::string_view text) {
    if (text.empty() || !(std::isalpha(static_cast<unsigned char>(text.front())) || text.front() == '_')) {
        return false;
    }
    return std::all_of(text.begin() + 1, text.end(), [](unsigned char ch) {
        return std::isalnum(ch) || ch == '_';
    });
}

[[noreturn]] void fail(const SourceLine& line, const std::string& message) {
    throw AssemblerError{"", line.number, message};
}

// Stage 2: remove comments and blank lines while retaining original line numbers.
std::vector<SourceLine> preprocess(std::string_view source) {
    std::vector<SourceLine> result;                     //this will hold cleaned lines
    std::istringstream input{std::string(source)};      //turns source text into input stream      
    std::string text;                                   //holds the current line
    unsigned number = 0;                                //number tracks the original line number.
    while (std::getline(input, text)) {
        ++number;
        const auto comment = text.find('@');
        if (comment != std::string::npos) {
            text.erase(comment);
        }
        text = trim(std::move(text));
        if (!text.empty()) {
            result.push_back(SourceLine{number, std::move(text), ""});
        }
    }
    return result;
}

// Stage 2b: expand the stack macros (full-descending stack, sp = r14):
//   push rX  ->  sub sp, sp, 4    then  st rX, 0[sp]
//   pop rX   ->  add sp, sp, 4    then  ld rX, -4[sp]
// sp moves first, so a push on a full stack raises StackOverflow and a pop on an empty
// stack raises StackUnderflow before memory is touched. A label stays on the first instruction.
std::vector<SourceLine> expand_macros(const std::vector<SourceLine>& lines) {
    std::vector<SourceLine> result;
    for (const SourceLine& line : lines) {
        std::string label;
        std::string rest = line.text;
        const auto colon = rest.find(':');
        if (colon != std::string::npos) {
            label = rest.substr(0, colon + 1) + " ";
            rest = trim(rest.substr(colon + 1));
        }

        std::istringstream words{rest};
        std::string mnemonic;
        std::string reg;
        std::string extra;
        words >> mnemonic >> reg >> extra;
        mnemonic = lower(mnemonic);
        if (mnemonic != "push" && mnemonic != "pop") {
            result.push_back(line);
            continue;
        }
        if (reg.empty() || !extra.empty()) {
            fail(line, "'" + mnemonic + "' takes one register");
        }
        if (lower(reg) == "sp" || lower(reg) == "r14") {
            fail(line, "cannot " + mnemonic + " sp");
        }

        const std::string macro = mnemonic + " " + reg;
        if (mnemonic == "push") {
            result.push_back(SourceLine{line.number, label + "sub sp, sp, 4", macro});
            result.push_back(SourceLine{line.number, "st " + reg + ", 0[sp]", macro});
        } else {
            result.push_back(SourceLine{line.number, label + "add sp, sp, 4", macro});
            result.push_back(SourceLine{line.number, "ld " + reg + ", -4[sp]", macro});
        }
    }
    return result;
}

// Stage 3: tokenize assembly punctuation separately from names and numbers.
std::vector<std::string> tokenize(const SourceLine& line) {
    std::vector<std::string> tokens;    //stores final tokens
    std::string current;                //builds a word charachter by charachter

    const auto flush = [&]() {
        if (!current.empty()) {
            tokens.push_back(std::move(current));
            current.clear();
        }
    };

    for (char ch : line.text) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            flush();
        } else if (ch == ',' || ch == '[' || ch == ']' || ch == ':') {
            flush();
            tokens.emplace_back(1, ch);
        } else {
            current += ch;
        }
    }
    flush();
    return tokens;
}

//This searches the instruction table in isa_config.hpp.
const isa::InstructionInfo* find_instruction(std::string_view mnemonic) {
    for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
        if (info.mnemonic == mnemonic) {
            return &info;
        }
    }
    return nullptr;
}

//returns immediate modifier and pointer to instruction metadata.
std::pair<const isa::InstructionInfo*, isa::Modifier> parse_mnemonic(const SourceLine& line,
                                                                       std::string mnemonic) {
    mnemonic = lower(std::move(mnemonic));
    if (const auto* info = find_instruction(mnemonic)) {
        return {info, isa::Modifier::Default};
    }

    if (mnemonic.size() > 1) {
        const char suffix = mnemonic.back();
        isa::Modifier modifier;
        if (suffix == 'u') {
            modifier = isa::Modifier::Unsigned;
        } else if (suffix == 'h') {
            modifier = isa::Modifier::High;
        } else {
            fail(line, "unknown instruction '" + mnemonic + "'");
        }

        mnemonic.pop_back();
        if (const auto* info = find_instruction(mnemonic)) {
            //Branch instructions, no-operand instructions, and memory instructions do not permit u or h modifiers.
            if (info->operands == isa::Operands::Label || info->operands == isa::Operands::None ||
                info->operands == isa::Operands::RdMem) {
                fail(line, "immediate modifier is not valid for '" + std::string(info->mnemonic) + "'");
            }
            return {info, modifier};
        }
    }

    fail(line, "unknown instruction '" + mnemonic + "'");
}

// Stage 5: recognise architectural register names.
int parse_register(const SourceLine& line, std::string token) {
    token = lower(std::move(token));
    if (token == "sp") {
        return int(isa::REG_SP);
    }
    if (token == "ra") {
        return int(isa::REG_RA);
    }
    //normal regsiter starts with 'r'
    if (token.size() < 2 || token.front() != 'r') {
        fail(line, "expected register, got '" + token + "'");
    }

    try {
        std::size_t consumed = 0;
        const int value = std::stoi(token.substr(1), &consumed, 10);
        if (consumed != token.size() - 1 || value < 0 || value >= int(isa::NUM_REGS)) {
            fail(line, "register '" + token + "' is outside r0-r15");
        }
        return value;
    } catch (const std::exception&) {
        fail(line, "expected register, got '" + token + "'");
    }
}

// Stage 6: parse decimal and 0x-prefixed integer literals before range checking.
std::int64_t parse_immediate(const SourceLine& line, const std::string& token) {
    try {
        std::size_t consumed = 0;
        const std::int64_t value = std::stoll(token, &consumed, 0);
        if (consumed != token.size()) {
            fail(line, "invalid immediate '" + token + "'");
        }
        return value;
    } catch (const std::exception&) {
        fail(line, "invalid immediate '" + token + "'");
    }
}

//creand an operand whose type is regsiter
Operand register_operand(const SourceLine& line, const std::string& token) {
    return Operand{OperandKind::Register, parse_register(line, token), 0, ""};
}

//create an immediate operand
Operand immediate_operand(const SourceLine& line, const std::string& token) {
    return Operand{OperandKind::Immediate, 0, parse_immediate(line, token), ""};
}

Operand label_operand(const SourceLine& line, const std::string& token) {
    const std::string name = lower(token);
    if (!is_identifier(name)) {
        fail(line, "expected label, got '" + token + "'");
    }
    return Operand{OperandKind::Label, 0, 0, name};
}

bool is_register_token(std::string token) {
    token = lower(std::move(token));
    if (token == "sp" || token == "ra") {
        return true;
    }
    if (token.size() < 2 || token.front() != 'r') {
        return false;
    }
    return std::all_of(token.begin() + 1, token.end(), [](unsigned char ch) { return std::isdigit(ch); });
}

std::vector<std::string> remove_commas(const std::vector<std::string>& tokens, std::size_t start) {
    std::vector<std::string> result;
    for (std::size_t i = start; i < tokens.size(); ++i) {
        if (tokens[i] != ",") {
            result.push_back(tokens[i]);
        }
    }
    return result;
}

// Stage 4: turn tokens into an instruction-shaped AST. Register and immediate
// values are converted here by the dedicated parsing helpers above.
// to convert tokens into structured instruction
Instruction parse_instruction(const SourceLine& line, const std::vector<std::string>& tokens,
                              std::size_t mnemonic_index) {
    const auto [info, modifier] = parse_mnemonic(line, tokens[mnemonic_index]);
    const std::vector<std::string> args = remove_commas(tokens, mnemonic_index + 1);
    Instruction instruction;
    instruction.info = info;
    instruction.modifier = modifier;

    const auto source_operand = [&](const std::string& token) {
        if (is_register_token(token)) {
            return register_operand(line, token);
        }
        return immediate_operand(line, token);
    };

    switch (info->operands) {
        case isa::Operands::RdRs1Src2:
            if (args.size() != 3) {
                fail(line, "'" + std::string(info->mnemonic) + "' expects rd, rs1, rs2/immediate");
            }
            instruction.operands = {register_operand(line, args[0]), register_operand(line, args[1]),
                                    source_operand(args[2])};
            break;
        case isa::Operands::Rs1Src2:
            if (args.size() != 2) {
                fail(line, "'cmp' expects rs1, rs2/immediate");
            }
            instruction.operands = {register_operand(line, args[0]), source_operand(args[1])};
            break;
        case isa::Operands::RdSrc2:
            if (args.size() != 2) {
                fail(line, "'mov'/'not' expects rd, rs2/immediate");
            }
            instruction.operands = {register_operand(line, args[0]), source_operand(args[1])};
            break;
        case isa::Operands::RdMem:
            if (args.size() != 5 || args[2] != "[" || args[4] != "]") {
                fail(line, "'" + std::string(info->mnemonic) + "' expects rd, offset[rs1]");
            }
            instruction.operands = {register_operand(line, args[0]), immediate_operand(line, args[1]),
                                    register_operand(line, args[3])};
            break;
        case isa::Operands::Label:
            if (args.size() != 1) {
                fail(line, "'" + std::string(info->mnemonic) + "' expects one label");
            }
            instruction.operands = {label_operand(line, args[0])};
            break;
        case isa::Operands::None:
            if (!args.empty()) {
                fail(line, "'" + std::string(info->mnemonic) + "' takes no operands");
            }
            break;
    }

    const bool has_immediate = std::any_of(instruction.operands.begin(), instruction.operands.end(),
                                           [](const Operand& operand) {
                                               return operand.kind == OperandKind::Immediate;
                                           });
    if (modifier != isa::Modifier::Default && !has_immediate) {
        fail(line, "immediate modifier requires an immediate operand");
    }
    return instruction;
}

std::vector<ParsedLine> parse_lines(const std::vector<SourceLine>& source) {
    std::vector<ParsedLine> parsed;
    for (const SourceLine& line : source) {
        const std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty()) {
            continue;
        }

        ParsedLine parsed_line;
        parsed_line.source = line;
        std::size_t index = 0;
        if (tokens.size() >= 2 && tokens[1] == ":") {
            const std::string name = lower(tokens[0]);
            if (!is_identifier(name)) {
                fail(line, "invalid label '" + tokens[0] + "'");
            }
            parsed_line.label = name;
            index = 2;
        }
        //if tokens remain after label, parse the instruction
        if (index < tokens.size()) {
            parsed_line.instruction = parse_instruction(line, tokens, index);
            parsed_line.has_instruction = true;
        }
        //Rejects a line that is neither a valid label nor an instruction.
        if (index == tokens.size() && parsed_line.label.empty()) {
            fail(line, "expected instruction or label");
        }
        parsed.push_back(std::move(parsed_line));
    }
    return parsed;
}

//Pass1 does not generate te machine code. It just finds the address for each label.

// Stage 7: assign addresses and collect labels before any branch is encoded.
SymbolTable first_pass(std::vector<ParsedLine>& lines) {
    SymbolTable symbols;
    Word location = 0;
    for (ParsedLine& line : lines) {
        line.address = location;
        if (!line.label.empty()) {
            const auto [_, inserted] = symbols.emplace(line.label, location);
            if (!inserted) {
                fail(line.source, "duplicate label '" + line.label + "'");
            }
        }
        //duplicate labels are invalid
        if (line.has_instruction) {
            if (location > std::numeric_limits<Word>::max() - isa::WORD_BYTES) {
                fail(line.source, "program is too large");
            }
            location += isa::WORD_BYTES;
        }
    }
    return symbols;
}


//Turns a parsed 64-bit immediate into the 16-bit field stored in an instruction.
Word checked_immediate(const SourceLine& line, std::int64_t value, isa::Modifier modifier) {
    if (modifier == isa::Modifier::Default) {
        if (value < std::numeric_limits<std::int16_t>::min() ||
            value > std::numeric_limits<std::int16_t>::max()) {
            fail(line, "signed immediate is outside -32768..32767");
        }
    } else if (value < 0 || value > std::numeric_limits<std::uint16_t>::max()) {
        fail(line, "unsigned/high immediate is outside 0..65535");
    }
    return Word(value) & ((Word{1} << isa::IMM_BITS) - 1);
}

//to place the 5 bits opcode into instruction bits 31:27
Word opcode_bits(isa::Opcode opcode) {
    return Word(opcode) << isa::OPCODE_POS;
}

Word encode_branch(const ParsedLine& line, const SymbolTable& symbols) {
    const Operand& target = line.instruction.operands.front();
    const auto found = symbols.find(target.label);
    if (found == symbols.end()) {
        fail(line.source, "undefined label '" + target.label + "'");
    }

    const std::int64_t delta = std::int64_t(found->second) - std::int64_t(line.address);
    if (delta % isa::WORD_BYTES != 0) {
        fail(line.source, "branch target is not word aligned");
    }
    const std::int64_t offset = delta / isa::WORD_BYTES;
    const std::int64_t minimum = -(std::int64_t{1} << (isa::OFFSET_BITS - 1));
    const std::int64_t maximum = (std::int64_t{1} << (isa::OFFSET_BITS - 1)) - 1;
    if (offset < minimum || offset > maximum) {
        fail(line.source, "branch target is outside the signed 27-bit offset range");
    }
    const Word mask = (Word{1} << isa::OFFSET_BITS) - 1;
    return opcode_bits(line.instruction.info->opcode) | (Word(offset) & mask);
}

// Stages 8 and 9: encode ordinary fields, then resolve PC-relative branches.
Word encode_instruction(const ParsedLine& line, const SymbolTable& symbols) {
    const Instruction& instruction = line.instruction;
    const auto operands = instruction.operands;
    if (instruction.info->operands == isa::Operands::Label) {
        return encode_branch(line, symbols);
    }

    Word word = opcode_bits(instruction.info->opcode);
    const auto encode_register_or_immediate = [&](const Operand& operand, unsigned rs2_pos) {
        if (operand.kind == OperandKind::Register) {
            word |= Word(operand.reg) << rs2_pos;
        } else {
            word |= Word{1} << isa::I_POS;
            word |= Word(instruction.modifier) << isa::MOD_POS;
            word |= checked_immediate(line.source, operand.immediate, instruction.modifier);
        }
    };

    //encoding each operand format.
    switch (instruction.info->operands) {
        case isa::Operands::RdRs1Src2:
            word |= Word(operands[0].reg) << isa::RD_POS;
            word |= Word(operands[1].reg) << isa::RS1_POS;
            encode_register_or_immediate(operands[2], isa::RS2_POS);
            break;
        case isa::Operands::Rs1Src2:
            word |= Word(operands[0].reg) << isa::RS1_POS;
            encode_register_or_immediate(operands[1], isa::RS2_POS);
            break;
        case isa::Operands::RdSrc2:
            word |= Word(operands[0].reg) << isa::RD_POS;
            encode_register_or_immediate(operands[1], isa::RS2_POS);
            break;
        case isa::Operands::RdMem:
            word |= Word{1} << isa::I_POS;
            word |= Word(operands[0].reg) << isa::RD_POS;
            word |= Word(operands[2].reg) << isa::RS1_POS;
            word |= checked_immediate(line.source, operands[1].immediate, isa::Modifier::Default);
            break;
        case isa::Operands::None:
            break;
        case isa::Operands::Label:
            break;
    }
    return word;
}

std::string read_source_file(const std::string& input_path) {
    std::ifstream input(input_path);
    if (!input) {
        throw AssemblerError{input_path, 0, "cannot open assembly source"};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        throw AssemblerError{input_path, 0, "failed while reading assembly source"};
    }
    return buffer.str();
}

} // namespace

AssemblerError::AssemblerError(std::string filename, unsigned line, std::string message)
    : std::runtime_error(filename.empty() ? message
                                           : filename + (line == 0 ? ": " : ":" + std::to_string(line) + ": ") + message),
      file(std::move(filename)), line(line) {}

std::vector<isa::Word> Assembler::assemble_file(const std::string& input_path) const {
    return assemble(read_source_file(input_path), input_path);
}

std::vector<isa::Word> Assembler::assemble(std::string_view source, const std::string& source_name) const {
    std::vector<isa::Word> words;
    for (const ListingLine& entry : assemble_listing(source, source_name)) {
        if (entry.has_instruction) {
            words.push_back(entry.word);
        }
    }
    return words;
}

std::vector<ListingLine> Assembler::assemble_listing_file(const std::string& input_path) const {
    return assemble_listing(read_source_file(input_path), input_path);
}

std::vector<ListingLine> Assembler::assemble_listing(std::string_view source, const std::string& source_name) const {
    try {
        const std::vector<SourceLine> cleaned = expand_macros(preprocess(source));
        std::vector<ParsedLine> parsed = parse_lines(cleaned);
        const SymbolTable symbols = first_pass(parsed);

        std::vector<ListingLine> listing;
        for (const ParsedLine& line : parsed) {
            ListingLine entry;
            entry.line = line.source.number;
            entry.text = line.source.text;
            entry.macro = line.source.macro;
            entry.has_instruction = line.has_instruction;
            entry.address = line.address;
            if (line.has_instruction) {
                entry.word = encode_instruction(line, symbols);
            }
            listing.push_back(entry);
        }
        return listing;
    } catch (const AssemblerError& error) {
        if (!error.filename().empty() || source_name.empty()) {
            throw;
        }
        throw AssemblerError{source_name, error.line_number(), error.what()};
    }
}

void Assembler::write_hex_file(const std::string& output_path, const std::vector<isa::Word>& words) const {
    std::ofstream output(output_path);
    if (!output) {
        throw AssemblerError{output_path, 0, "cannot open output file"};
    }
    for (Word word : words) {
        output << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << word << '\n';
    }
    if (!output) {
        throw AssemblerError{output_path, 0, "failed while writing output file"};
    }
}

} // namespace risc::assembler
