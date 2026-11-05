#pragma once

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "risc/assembler/assembler.hpp"
#include "risc/memory/memory.hpp"
#include "risc/microassembler/microassembler.hpp"
#include "risc/microassembler/splitter.hpp"
#include "risc/six-stage-micro/cpu.hpp"

namespace risc::web {

using nlohmann::json;

// What one stage worked on during a beat
struct BeatCell {
    bool empty = true; // no instruction (a bubble, or IF did not fetch)
    std::uint64_t id = 0; // the instruction's packet id (0 if it could not be fetched)
    isa::Word pc = 0;
    isa::Word ir = 0;
    bool waited = false; // a data hazard made it wait this beat
    bool exception = false; // it carries an exception
};

// One row of the beat log
struct BeatRecord {
    std::uint64_t number = 0; // 1 for the first beat
    std::uint64_t microcycles = 0;
    std::array<BeatCell, six_stage_micro::NUM_STAGES> cells;
    bool taken = false; // a branch was taken in EX
    isa::Word target = 0;
};

// The program and the six-stage processor shown in the web interface
class Session {
private:
    std::string microcode_path;
    microassembler::Microprogram microprogram;
    std::array<microassembler::StageMicroprogram, six_stage_micro::NUM_STAGES> stage_programs;
    std::vector<std::string> microcode_lines;

    std::string program_name;
    std::string program_source;
    std::vector<assembler::ListingLine> listing;
    std::vector<isa::Word> program;

    memory::Memory memory;
    six_stage_micro::CPU cpu{memory};

    // The beat being recorded for the beat log
    bool beat_open = false;
    BeatRecord current_beat;
    std::uint64_t beat_start_microcycle = 0;
    isa::Word beat_fetch_pc = 0;
    std::uint64_t beat_fetch_count = 0;
    std::uint64_t beat_taken = 0;
    std::vector<BeatRecord> beat_log;
    std::set<isa::Word> return_slots; // stack words whose last store came from ra, for the stack view

    void restart();
    void open_beat();
    void close_beat();

public:
    // Microassembles and splits the microcode. Throws MicroassemblerError.
    Session(const std::string& microcode_path);

    // Assembles a program and loads it. Throws AssemblerError (the old program stays loaded).
    void load(const std::string& name, const std::string& source);

    // Back to the start of the loaded program
    void reset();

    void step_microcycle();
    void step_beat();
    void run(std::uint64_t max_microcycles);

    // The program, the microcode and the stage microprograms (they change only on load)
    json program_json() const;

    // Everything that changes as the processor runs
    json state_json() const;
};
}
