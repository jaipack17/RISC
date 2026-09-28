#include <iostream>
#include <string>
#include <vector>

#include "risc/assembler/assembler.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: risc_asm <input.asm> [output.hex]\n";
        return 1;
    }

    const std::string input_path = argv[1];
    const std::string output_path = argc == 3 ? argv[2] : input_path + ".hex";

    try {
        risc::assembler::Assembler assembler;
        const std::vector<risc::isa::Word> words = assembler.assemble_file(input_path);
        assembler.write_hex_file(output_path, words);
        std::cout << "Assembled " << words.size() << " instruction(s) to " << output_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Assembler error: " << error.what() << '\n';
        return 1;
    }
}
