#pragma once

#include <array>
#include <string>

#include "nlohmann/json.hpp"
#include "risc/alu/alu_types.hpp"
#include "risc/four-stage-micro/pipeline.hpp"
#include "risc/microassembler/microassembler.hpp"
#include "risc/microassembler/splitter.hpp"
#include "risc/six-stage-micro/pipeline.hpp"

namespace risc::web {

using nlohmann::json;

// Runs one program to the end on fresh four-stage, six-stage and sequential microprogrammed
// processors and measures them, for the Compare page. The sessions are not touched.
class Comparer {
private:
    microassembler::Microprogram microprogram;
    std::array<microassembler::StageMicroprogram, four_stage_micro::NUM_STAGES> four_programs;
    std::array<microassembler::StageMicroprogram, six_stage_micro::NUM_STAGES> six_programs;

public:
    // Microassembles and splits the microcode. Throws MicroassemblerError.
    Comparer(const std::string& microcode_path);

    // Assembles the program (throws AssemblerError) and runs it on the three processors.
    // With timeline, the first beats of both pipelines are recorded instruction by instruction.
    // All three ALUs use the algorithms in alu_config.
    json compare(const std::string& source, bool timeline, const alu::AluConfig& alu_config = alu::AluConfig{}) const;
};
}
