#pragma once

#include <array>
#include <vector>

#include "risc/microassembler/microassembler.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/six-stage-micro/pipeline.hpp"

namespace risc::microassembler {

// The microcode of one pipeline stage
struct StageMicroprogram {
    std::vector<micro::MicroWord> words; // word i is stored at µaddress i of the stage's microprogram memory
    std::vector<unsigned> source_lines; // the .masm line of word i (0 for an end marker)
    std::vector<bool> parts; // true if word i is only part of its source line
    std::vector<Entry> entries; // the stage's switch table (empty for IF)
};

// Split a microprogram into the microprograms of the six stages, indexed by Stage
std::array<StageMicroprogram, six_stage_micro::NUM_STAGES> split(const Microprogram& program);
}
