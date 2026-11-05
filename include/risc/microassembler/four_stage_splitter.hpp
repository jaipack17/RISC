#pragma once

#include <array>

#include "risc/four-stage-micro/pipeline.hpp"
#include "risc/microassembler/microassembler.hpp"
#include "risc/microassembler/splitter.hpp"

namespace risc::microassembler {

// Split a microprogram into the microprograms of the four stages, indexed by four_stage_micro::Stage
std::array<StageMicroprogram, four_stage_micro::NUM_STAGES> split_four(const Microprogram& program);
}
