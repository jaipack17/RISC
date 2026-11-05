#pragma once

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "risc/assembler/assembler.hpp"
#include "risc/four-stage-micro/cpu.hpp"
#include "risc/memory/memory.hpp"
#include "risc/microassembler/four_stage_splitter.hpp"
#include "risc/microassembler/microassembler.hpp"
#include "session.hpp"

namespace risc::web {

using nlohmann::json;

// One row of the four-stage beat log (BeatCell is in session.hpp)
struct FourStageBeatRecord {
    std::uint64_t number = 0; // 1 for the first beat
    std::uint64_t microcycles = 0;
    std::array<BeatCell, four_stage_micro::NUM_STAGES> cells;
    bool taken = false; // a branch was taken in EX
    isa::Word target = 0;
};

// The program and the four-stage processor shown in the web interface
class FourStageSession {
private:
    std::string microcode_path;
    microassembler::Microprogram microprogram;
    std::array<microassembler::StageMicroprogram, four_stage_micro::NUM_STAGES> stage_programs;
    std::vector<std::string> microcode_lines;

    std::string program_name;
    std::string program_source;
    std::vector<assembler::ListingLine> listing;
    std::vector<isa::Word> program;

    memory::Memory memory;
    four_stage_micro::CPU cpu{memory};

    // The beat being recorded for the beat log
    bool beat_open = false;
    FourStageBeatRecord current_beat;
    std::uint64_t beat_start_microcycle = 0;
    isa::Word beat_fetch_pc = 0;
    std::uint64_t beat_fetch_count = 0;
    std::uint64_t beat_taken = 0;
    std::vector<FourStageBeatRecord> beat_log;
    std::set<isa::Word> return_slots; // stack words whose last store came from ra, for the stack view

    void restart();
    void open_beat();
    void close_beat();

public:
    // Microassembles and splits the microcode. Throws MicroassemblerError.
    FourStageSession(const std::string& microcode_path);

    // Assembles a program and loads it. Throws AssemblerError (the old program stays loaded).
    void load(const std::string& name, const std::string& source);

    // Back to the start of the loaded program
    void reset();

    // Which adder, multiplier and divider the ALU uses; starts the program again
    void configure_alu(const alu::AluConfig& config);

    void step_microcycle();
    void step_beat();
    void run(std::uint64_t max_microcycles);

    // The program, the microcode and the stage microprograms (they change only on load)
    json program_json() const;

    // Everything that changes as the processor runs
    json state_json() const;
};
}
