#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "risc/isa/isa_config.hpp"
#include "risc/four-stage-micro/pipeline.hpp"
#include "risc/microassembler/four_stage_splitter.hpp"
#include "risc/microassembler/microassembler.hpp"
#include "risc/microassembler/splitter.hpp"
#include "risc/six-stage-micro/pipeline.hpp"

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

const char* SIX_STAGE_NAMES[] = {"IF", "OF1", "OF2", "EX", "MA", "RW"};
const char* FOUR_STAGE_NAMES[] = {"IF", "OF", "EX", "MW"};

// The lines of a text file (lines[0] is line 1), without their indentation
std::vector<std::string> read_source_lines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        lines.push_back(line);
    }
    return lines;
}

// Prints each stage's microprogram, with the .masm line every word came from, and its switch table
void print_stages(const microassembler::StageMicroprogram* stages, unsigned count, const char* const* names,
                  const std::vector<std::string>& source) {
    for (unsigned s = 0; s < count; s++) {
        const microassembler::StageMicroprogram& stage = stages[s];
        std::printf("\n%s stage microprogram (%zu microinstructions):\n", names[s], stage.words.size());

        for (unsigned i = 0; i < stage.words.size(); i++) {
            std::printf("  µ%-4u 0x%012llX  ", i, (unsigned long long)stage.words[i]);
            unsigned line = stage.source_lines[i];
            if (line == 0) {
                std::printf("(end of section)\n");
            } else if (stage.parts[i]) {
                std::printf("line %-4u %-32s (%s part)\n", line, source[line - 1].c_str(), names[s]);
            } else {
                std::printf("line %-4u %s\n", line, source[line - 1].c_str());
            }
        }

        if (!stage.entries.empty()) {
            std::printf("%s switch table:\n", names[s]);
            for (const microassembler::Entry& entry : stage.entries) {
                std::printf("  %-5s -> µ%u\n", mnemonic_of(entry.opcode).c_str(), entry.address);
            }
        }
    }
}

// Usage: risc_masm [--stages | --stages4] <microcode.masm>
// Prints the microprogram memory contents, the switch table, and any warnings.
// With --stages it also prints the six pipeline stages' microprograms, with --stages4 the four stages'.
int main(int argc, char* argv[]) {
    bool six_stages = argc == 3 && std::string(argv[1]) == "--stages";
    bool four_stages = argc == 3 && std::string(argv[1]) == "--stages4";
    if (argc != 2 && !six_stages && !four_stages) {
        std::cerr << "Usage: risc_masm [--stages | --stages4] <microcode.masm>\n";
        return 1;
    }
    std::string path = argv[argc - 1];

    try {
        microassembler::Microassembler microassembler;
        microassembler::Microprogram program = microassembler.assemble_file(path);

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

        if (six_stages) {
            std::array<microassembler::StageMicroprogram, six_stage_micro::NUM_STAGES> stages =
                microassembler::split(program);
            print_stages(stages.data(), six_stage_micro::NUM_STAGES, SIX_STAGE_NAMES, read_source_lines(path));
        }
        if (four_stages) {
            std::array<microassembler::StageMicroprogram, four_stage_micro::NUM_STAGES> stages =
                microassembler::split_four(program);
            print_stages(stages.data(), four_stage_micro::NUM_STAGES, FOUR_STAGE_NAMES, read_source_lines(path));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Microassembler error: " << error.what() << '\n';
        return 1;
    }
}
