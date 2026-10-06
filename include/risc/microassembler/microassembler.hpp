#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "risc/isa/isa_config.hpp"
#include "risc/micro/control/microinstruction.hpp"

namespace risc::microassembler {

// A microassembly error, with the source line it was found on
class MicroassemblerError : public std::runtime_error {
public:
    unsigned line;

    MicroassemblerError(unsigned line, const std::string& message)
        : std::runtime_error(line == 0 ? message : "line " + std::to_string(line) + ": " + message), line(line) {}
};

// One switch table entry: instruction `opcode` starts at µaddress `address`
struct Entry {
    isa::Opcode opcode;
    unsigned address;
};

// Everything the microassembler produces
struct Microprogram {
    std::vector<micro::MicroWord> words; // word i is stored at µaddress i
    std::vector<Entry> entries; // the switch table
    std::vector<std::string> warnings; // e.g. opcodes with no microcode
};

class Microassembler {
public:
    Microprogram assemble_file(const std::string& path) const;
    Microprogram assemble(const std::string& source) const;
};
}
