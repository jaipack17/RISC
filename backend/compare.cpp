#include "compare.hpp"

#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

#include "json_util.hpp"
#include "risc/assembler/assembler.hpp"
#include "risc/exception.hpp"
#include "risc/four-stage-micro/cpu.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/control/micro_control_unit.hpp"
#include "risc/micro/datapath/datapath.hpp"
#include "risc/microassembler/four_stage_splitter.hpp"
#include "risc/six-stage-micro/cpu.hpp"

namespace risc::web {

namespace {

const std::uint64_t MAX_MICROCYCLES = 2'000'000;
const std::uint64_t TIMELINE_BEATS = 80; // beats recorded instruction by instruction

const char* FOUR_STAGE_NAMES[] = {"IF", "OF", "EX", "MW"};
const char* SIX_STAGE_NAMES[] = {"IF", "OF1", "OF2", "EX", "MA", "RW"};

// How a run ended, to check that the three processors agree
struct Outcome {
    bool halted = false;
    bool exception = false;
    CpuException error{};
    isa::Word exception_pc = 0;
    std::array<isa::Word, isa::NUM_REGS> regs{};
    bool flagE = false;
    bool flagGT = false;
};

json outcome_json(const Outcome& o) {
    json j;
    j["status"] = "limit";
    if (o.halted) {
        j["status"] = "halted";
    } else if (o.exception) {
        j["status"] = "exception";
    }
    j["exception"] = nullptr;
    if (o.exception) {
        j["exception"]["kind"] = exception_name(o.error);
        j["exception"]["pc"] = o.exception_pc;
    }
    return j;
}

// One instruction's row of the space-time diagram
struct TimelineRow {
    isa::Word pc = 0;
    isa::Word ir = 0;
    json cells = json::array(); // [beat, stage, waited]
};

// Runs a pipelined processor to the end, beat by beat, and measures it.
// Each beat: its length, which stage was the longest (the critical stage), how long every stage
// was busy, and which stage waited for a data hazard.
template <typename Cpu, typename Stage, unsigned N>
json run_pipeline(Cpu& cpu, const memory::Memory& memory, const char* const* names,
                  const std::array<microassembler::StageMicroprogram, N>& programs, bool timeline,
                  Outcome& outcome) {
    std::array<std::uint64_t, N> busy{};
    std::array<std::uint64_t, N> critical{};
    std::array<std::uint64_t, N> waits{};
    std::map<std::uint64_t, std::uint64_t> lengths;
    // The cycle stack: each beat's µcycles go to what leaves the last stage in it, an instruction
    // finishing (work) or a bubble, charged to why that slot is empty. The four add up to the total.
    std::array<std::uint64_t, 4> stack_microcycles{}; // work, stall, flush, fill and drain
    std::array<std::uint64_t, 4> stack_beats{};
    json beat_rows = json::array();
    std::map<std::uint64_t, TimelineRow> rows;
    std::uint64_t beats = 0;

    while (cpu.status == decltype(cpu.status)::Running && cpu.microcycles < MAX_MICROCYCLES) {
        bool record = timeline && beats < TIMELINE_BEATS;
        std::array<std::uint64_t, N> held{}; // the instruction id each stage starts the beat with
        for (unsigned s = 1; s < N; s++) {
            const auto& p = cpu.datapath.packet_of(Stage(s));
            if (!p.bubble && p.id > 0 && record) {
                held[s] = p.id;
                rows[p.id].pc = p.currentPc;
                rows[p.id].ir = p.ir;
            }
        }
        const auto& last = cpu.datapath.packet_of(Stage(N - 1)); // the instruction that finishes this beat
        unsigned slot = 0;
        if (last.bubble && last.reason == decltype(last.reason)::Stall) {
            slot = 1;
        } else if (last.bubble && last.reason == decltype(last.reason)::Flush) {
            slot = 2;
        } else if (last.bubble) {
            slot = 3; // empty since the start, or nothing fetched after a hlt
        }

        std::uint64_t beat = cpu.beats;
        std::uint64_t start = cpu.microcycles;
        std::uint64_t fetch_count = cpu.datapath.fetch_count;
        std::uint64_t taken = cpu.taken_branches;
        isa::Word fetch_pc = cpu.datapath.fetch.pc;

        std::array<std::uint64_t, N> ran{};
        while (cpu.status == decltype(cpu.status)::Running && cpu.beats == beat &&
               cpu.microcycles < MAX_MICROCYCLES) {
            cpu.step();
            for (unsigned s = 0; s < N; s++) {
                if (cpu.stages[s].ran) {
                    ran[s]++;
                }
            }
        }
        std::uint64_t length = cpu.microcycles - start;
        if (length == 0) {
            break; // the exception was reported before the beat started
        }
        beats++;
        lengths[length]++;
        stack_microcycles[slot] += length;
        stack_beats[slot]++;

        unsigned longest = 0;
        for (unsigned s = 0; s < N; s++) {
            busy[s] += ran[s];
            if (ran[s] > ran[longest]) {
                longest = s;
            }
        }
        if (ran[longest] > 0) {
            critical[longest]++;
        }
        int stall_stage = -1;
        if (cpu.stall) {
            stall_stage = int(cpu.stall_stage);
            waits[unsigned(cpu.stall_stage)]++;
        }

        if (record) {
            if (cpu.fetching && cpu.datapath.fetch_count > fetch_count) {
                held[0] = cpu.datapath.fetch_count;
                rows[held[0]].pc = fetch_pc;
                rows[held[0]].ir = memory.read_word(fetch_pc);
            }
            for (unsigned s = 0; s < N; s++) {
                if (held[s] > 0) {
                    bool waited = stall_stage >= 0 && int(s) <= stall_stage;
                    rows[held[s]].cells.push_back(json::array({beats, s, waited}));
                }
            }
            json b;
            b["beat"] = beats;
            b["microcycles"] = length;
            b["stall"] = stall_stage;
            b["taken"] = cpu.taken_branches != taken;
            beat_rows.push_back(b);
        }
    }

    outcome.halted = cpu.status == decltype(cpu.status)::Halted;
    outcome.exception = cpu.status == decltype(cpu.status)::Exception;
    outcome.error = cpu.exception;
    outcome.exception_pc = cpu.exception_pc;
    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        outcome.regs[r] = cpu.datapath.register_file.value(r);
    }
    outcome.flagE = cpu.datapath.alu.flagE;
    outcome.flagGT = cpu.datapath.alu.flagGT;

    json j = outcome_json(outcome);
    j["microcycles"] = cpu.microcycles;
    j["beats"] = beats;
    j["instructions"] = cpu.instructions;
    j["fetched"] = cpu.datapath.fetch_count;
    j["stallBeats"] = cpu.stall_beats;
    j["takenBranches"] = cpu.taken_branches;
    j["stages"] = json::array();
    for (unsigned s = 0; s < N; s++) {
        json st;
        st["name"] = names[s];
        st["busy"] = busy[s];
        st["critical"] = critical[s];
        st["waits"] = waits[s];
        st["controlStoreWords"] = programs[s].words.size();
        j["stages"].push_back(st);
    }
    const char* stack_names[] = {"work", "stall", "flush", "fillDrain"};
    for (unsigned k = 0; k < 4; k++) {
        j["cycleStack"][stack_names[k]]["microcycles"] = stack_microcycles[k];
        j["cycleStack"][stack_names[k]]["beats"] = stack_beats[k];
    }
    j["beatLengths"] = json::array();
    for (const auto& [length, count] : lengths) {
        j["beatLengths"].push_back(json{{"length", length}, {"count", count}});
    }
    if (timeline) {
        j["timeline"]["beats"] = beat_rows;
        j["timeline"]["rows"] = json::array();
        for (const auto& [id, row] : rows) {
            json r;
            r["id"] = id;
            r["pc"] = row.pc;
            r["assembly"] = disassemble(row.pc, row.ir);
            r["cells"] = row.cells;
            j["timeline"]["rows"].push_back(r);
        }
    }
    return j;
}

} // namespace

Comparer::Comparer(const std::string& microcode_path) {
    microassembler::Microassembler microassembler;
    microprogram = microassembler.assemble_file(microcode_path);
    four_programs = microassembler::split_four(microprogram);
    six_programs = microassembler::split(microprogram);
}

json Comparer::compare(const std::string& source, bool timeline) const {
    assembler::Assembler assembler;
    std::vector<isa::Word> program = assembler.assemble(source, "");
    if (program.size() * isa::WORD_BYTES > machine::INSTRUCTION_END - machine::INSTRUCTION_BASE) {
        throw std::runtime_error("the program does not fit in the instruction region");
    }

    // On the heap: the server runs this on a thread with a small stack
    auto four_memory = std::make_unique<memory::Memory>();
    auto six_memory = std::make_unique<memory::Memory>();
    auto seq_memory = std::make_unique<memory::Memory>();
    for (unsigned i = 0; i < program.size(); i++) {
        isa::Word address = machine::INSTRUCTION_BASE + i * isa::WORD_BYTES;
        four_memory->write_instruction_word(address, program[i]);
        six_memory->write_instruction_word(address, program[i]);
        seq_memory->write_instruction_word(address, program[i]);
    }

    auto four = std::make_unique<four_stage_micro::CPU>(*four_memory);
    four->write_stage_microprograms(four_programs);
    auto six = std::make_unique<six_stage_micro::CPU>(*six_memory);
    six->write_stage_microprograms(six_programs);

    Outcome four_outcome;
    Outcome six_outcome;
    json j;
    j["four"] = run_pipeline<four_stage_micro::CPU, four_stage_micro::Stage, four_stage_micro::NUM_STAGES>(
        *four, *four_memory, FOUR_STAGE_NAMES, four_programs, timeline, four_outcome);
    j["six"] = run_pipeline<six_stage_micro::CPU, six_stage_micro::Stage, six_stage_micro::NUM_STAGES>(
        *six, *six_memory, SIX_STAGE_NAMES, six_programs, timeline, six_outcome);

    // The sequential microprogrammed processor
    auto seq_datapath = std::make_unique<micro::Datapath>();
    auto seq_control = std::make_unique<micro::MicroControlUnit>();
    seq_control->write_microprogram_words(microprogram.words);
    for (const microassembler::Entry& entry : microprogram.entries) {
        seq_control->switch_unit.set_start(entry.opcode, entry.address);
    }
    Outcome seq_outcome;
    std::uint64_t seq_microcycles = 0;
    std::uint64_t seq_instructions = 0;
    isa::Word instruction_pc = 0;
    try {
        while (!seq_control->halted && seq_microcycles < MAX_MICROCYCLES) {
            if (seq_control->upc == 0) {
                instruction_pc = seq_datapath->fetch.pc; // the preamble starts at µaddress 0
                seq_instructions++;
            }
            seq_control->step(*seq_datapath, *seq_memory);
            seq_microcycles++;
        }
    } catch (const CpuException& e) {
        seq_outcome.exception = true;
        seq_outcome.error = e;
        seq_outcome.exception_pc = instruction_pc;
        seq_microcycles++;
        seq_instructions--; // the instruction that raised it did not finish
    }
    seq_outcome.halted = seq_control->halted;
    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        seq_outcome.regs[r] = seq_datapath->register_file.value(r);
    }
    seq_outcome.flagE = seq_datapath->alu.flagE;
    seq_outcome.flagGT = seq_datapath->alu.flagGT;
    j["sequential"] = outcome_json(seq_outcome);
    j["sequential"]["microcycles"] = seq_microcycles;
    j["sequential"]["instructions"] = seq_instructions;
    j["sequential"]["controlStoreWords"] = microprogram.words.size();

    // All three must end in the same state
    std::string differences;
    for (const Outcome* o : {&six_outcome, &seq_outcome}) {
        if (o->halted != four_outcome.halted || o->exception != four_outcome.exception) {
            differences += " status";
        }
        if (o->exception && four_outcome.exception &&
            (o->error.kind != four_outcome.error.kind || o->exception_pc != four_outcome.exception_pc)) {
            differences += " exception";
        }
        if (o->regs != four_outcome.regs) {
            differences += " registers";
        }
        if (o->flagE != four_outcome.flagE || o->flagGT != four_outcome.flagGT) {
            differences += " flags";
        }
    }
    for (isa::Word address = machine::DATA_BASE; address < machine::MEM_SIZE; address += isa::WORD_BYTES) {
        isa::Word value = four_memory->read_word(address);
        if (value != six_memory->read_word(address) || value != seq_memory->read_word(address)) {
            differences += " memory";
            break;
        }
    }
    j["agree"] = differences.empty();
    j["differences"] = differences;

    j["registers"] = four_outcome.regs;
    return j;
}
}
