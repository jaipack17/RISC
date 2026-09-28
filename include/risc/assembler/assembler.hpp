#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "risc/isa/isa_config.hpp"

namespace risc::assembler {

// An assembly error always identifies the source line when one exists.
class AssemblerError : public std::runtime_error {
public:
    AssemblerError(std::string filename, unsigned line, std::string message);

    const std::string& filename() const { return file; }
    unsigned line_number() const { return line; }

private:
    std::string file;
    unsigned line = 0;
};

// Two-pass RISC201 assembler. The input is assembly source; the result is one
// 32-bit instruction word per source instruction, ready for memory::load_word.
class Assembler {
public:
    std::vector<isa::Word> assemble_file(const std::string& input_path) const;
    std::vector<isa::Word> assemble(std::string_view source,
                                    const std::string& source_name = "<input>") const;

    // Writes one eight-digit hexadecimal word per line. This is a text
    // representation of the same 32-bit values executed by the CPU.
    void write_hex_file(const std::string& output_path,
                        const std::vector<isa::Word>& words) const;
};

} // namespace risc::assembler
