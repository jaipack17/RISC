#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "risc/assembler/assembler.hpp"
#include "risc/disassembler/disassembler.hpp"

using namespace risc;

// Assembles the disassembled text again and compares it with the original words.
// Returns how many words differ (or are missing), and prints each one to stderr.
unsigned check_round_trip(const std::string& text, const std::vector<isa::Word>& words) {
    assembler::Assembler assembler;
    std::vector<isa::Word> again = assembler.assemble(text, "<disassembly>");
    unsigned differences = 0;
    for (unsigned i = 0; i < words.size() || i < again.size(); i++) {
        isa::Word address = machine::INSTRUCTION_BASE + i * isa::WORD_BYTES;
        if (i >= words.size() || i >= again.size()) {
            std::fprintf(stderr, "  %04X: the reassembled program has %zu words, the original %zu\n", address,
                         again.size(), words.size());
            differences++;
            break;
        }
        if (words[i] != again[i]) {
            std::fprintf(stderr, "  %04X: %08X reassembles as %08X (bits the instruction does not use are set)\n",
                         address, words[i], again[i]);
            differences++;
        }
    }
    return differences;
}

// Usage: risc_disasm <input.hex> [output.asm]
// Turns the words risc_asm wrote back into assembly (to output.asm, or printed), then checks that
// assembling that text gives the same words.
int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: risc_disasm <input.hex> [output.asm]\n";
        return 1;
    }
    std::string input_path = argv[1];

    try {
        disassembler::Disassembler disassembler;
        std::vector<isa::Word> words = disassembler.read_hex_file(input_path);
        std::vector<disassembler::DisassembledLine> lines = disassembler.disassemble(words);
        std::string title = "disassembled from " + std::filesystem::path(input_path).filename().string() + " by risc_disasm (" +
                            std::to_string(words.size()) + " words)";
        std::string text = disassembler.source(lines, title);

        if (argc == 3) {
            std::ofstream output(argv[2]);
            if (!output) {
                std::cerr << argv[2] << ": cannot open\n";
                return 1;
            }
            output << text;
            std::cerr << "Disassembled " << words.size() << " word(s) to " << argv[2] << '\n';
        } else {
            std::cout << text;
        }

        unsigned problems = 0;
        for (const disassembler::DisassembledLine& line : lines) {
            if (!line.problem.empty()) {
                std::fprintf(stderr, "  %04X: %08X cannot be disassembled: %s\n", line.address, line.word, line.problem.c_str());
                problems++;
            }
        }
        if (problems > 0) {
            std::cerr << "Check FAILED: " << problems << " word(s) are not instructions the assembler can write, so the "
                      << "text does not assemble to the same program.\n";
            return 1;
        }
        unsigned differences = check_round_trip(text, words);
        if (differences > 0) {
            std::cerr << "Check FAILED: " << differences << " word(s) differ after assembling the text again.\n";
            return 1;
        }
        std::cerr << "Check: assembling the text again gives the same " << words.size() << " word(s).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Disassembler error: " << error.what() << '\n';
        return 1;
    }
}
