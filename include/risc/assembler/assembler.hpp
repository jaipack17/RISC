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

// One source line after assembly, for listings and the visualizer
struct ListingLine {
    unsigned line = 0; // source line number
    std::string text; // source text without the comment, e.g. "fill: st r2, 0[r1]"
    bool has_instruction = false; // false for a label-only line such as "sum_loop:"
    isa::Word address = 0; // where the instruction is in memory
    isa::Word word = 0; // its machine code
};

// Two-pass RISC201 assembler. The input is assembly source; the result is one
// 32-bit instruction word per source instruction, ready for memory::write_instruction_word.
class Assembler {
public:
    std::vector<isa::Word> assemble_file(const std::string& input_path) const;
    std::vector<isa::Word> assemble(std::string_view source,
                                    const std::string& source_name = "<input>") const;

    // Same as above, but keeps every source line with its address and machine code
    std::vector<ListingLine> assemble_listing_file(const std::string& input_path) const;
    std::vector<ListingLine> assemble_listing(std::string_view source,
                                              const std::string& source_name = "<input>") const;

    // Writes one eight-digit hexadecimal word per line. This is a text
    // representation of the same 32-bit values executed by the CPU.
    void write_hex_file(const std::string& output_path,
                        const std::vector<isa::Word>& words) const;
};

} // namespace risc::assembler
