#pragma once

#include <string>
#include <vector>

#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"

namespace risc::disassembler {

// One word of the program, disassembled
struct DisassembledLine {
    isa::Word address = 0;
    isa::Word word = 0;
    std::string label;   // the label that marks this address, e.g. "L_0010" ("" if none)
    std::string text;    // e.g. "add r1, r2, 5" ("" if the word cannot be written as assembly)
    std::string problem; // why it cannot be ("" if it can)
};

// One instruction as assembly text, with branch_operand as the operand of a branch or call.
// "" if the word has no instruction's opcode.
std::string instruction_text(isa::Word word, const std::string& branch_operand);

// Where a branch or call at address goes
isa::Word branch_target(isa::Word address, isa::Word word);

// Turns machine code back into assembly that the assembler turns into the same words
class Disassembler {
public:
    // The words of a program loaded at base, one line each. Branch and call targets get labels:
    // "f_0010" for a call target, "L_0010" for any other.
    std::vector<DisassembledLine> disassemble(const std::vector<isa::Word>& words,
                                              isa::Word base = machine::INSTRUCTION_BASE) const;

    // The lines as assembly source, each with its address and word in a comment
    std::string source(const std::vector<DisassembledLine>& lines, const std::string& title) const;

    // Reads a file of 8-digit hexadecimal words, one per line, as risc_asm writes it.
    // Throws std::runtime_error naming the line if one is not a word.
    std::vector<isa::Word> read_hex_file(const std::string& path) const;
};
}
