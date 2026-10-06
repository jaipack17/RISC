#include <cstdio>
#include <iostream>
#include <string>

#include "risc/isa/isa_config.hpp"
#include "risc/microassembler/microassembler.hpp"

using namespace risc;

// The mnemonic of an opcode, e.g. "add"
std::string mnemonic_of(isa::Opcode opcode) {
    for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
        if (info.opcode == opcode) {
            return std::string(info.mnemonic);
        }
    }
    return "?";
}

// Usage: risc_masm <microcode.masm>
// Prints the microprogram memory contents, the switch table, and any warnings.
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: risc_masm <microcode.masm>\n";
        return 1;
    }

    try {
        microassembler::Microassembler microassembler;
        microassembler::Microprogram program = microassembler.assemble_file(argv[1]);

        std::printf("Microprogram memory (%zu microinstructions):\n", program.words.size());
        for (unsigned i = 0; i < program.words.size(); i++) {
            std::printf("  µ%-4u 0x%012llX\n", i, (unsigned long long)program.words[i]);
        }

        std::printf("\nSwitch table:\n");
        for (const microassembler::Entry& entry : program.entries) {
            std::printf("  %-5s -> µ%u\n", mnemonic_of(entry.opcode).c_str(), entry.address);
        }

        if (!program.warnings.empty()) {
            std::printf("\nWarnings:\n");
            for (const std::string& warning : program.warnings) {
                std::printf("  %s\n", warning.c_str());
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Microassembler error: " << error.what() << '\n';
        return 1;
    }
}
