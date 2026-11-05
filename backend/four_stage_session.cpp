#include "four_stage_session.hpp"

#include <fstream>
#include <stdexcept>

#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/micro/control/micro_decode_unit.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/micro/micro_isa.hpp"
#include "json_util.hpp"

namespace risc::web {

using four_stage_micro::CpuStatus;
using four_stage_micro::InstructionPacket;
using four_stage_micro::NUM_STAGES;
using four_stage_micro::Stage;

namespace {

const char* STAGE_NAMES[] = {"IF", "OF", "EX", "MW"};

json beat_json(const FourStageBeatRecord& beat) {
    json j;
    j["beat"] = beat.number;
    j["microcycles"] = beat.microcycles;
    j["taken"] = beat.taken;
    j["target"] = beat.target;
    j["cells"] = json::array();
    for (const BeatCell& cell : beat.cells) {
        json c;
        c["empty"] = cell.empty;
        c["id"] = cell.id;
        c["pc"] = cell.pc;
        c["assembly"] = "";
        if (!cell.empty && !cell.exception) {
            c["assembly"] = disassemble(cell.pc, cell.ir);
        }
        c["waited"] = cell.waited;
        c["exception"] = cell.exception;
        j["cells"].push_back(c);
    }
    return j;
}

const char* status_name(CpuStatus status) {
    if (status == CpuStatus::Halted) {
        return "halted";
    }
    if (status == CpuStatus::Exception) {
        return "exception";
    }
    return "running";
}

} // namespace

FourStageSession::FourStageSession(const std::string& microcode_path) : microcode_path(microcode_path) {
    microassembler::Microassembler microassembler;
    microprogram = microassembler.assemble_file(microcode_path);
    stage_programs = microassembler::split_four(microprogram);
    cpu.write_stage_microprograms(stage_programs);

    std::ifstream file(microcode_path);
    std::string line;
    while (std::getline(file, line)) {
        microcode_lines.push_back(line);
    }
    restart();
}

void FourStageSession::load(const std::string& name, const std::string& source) {
    assembler::Assembler assembler;
    std::vector<assembler::ListingLine> new_listing = assembler.assemble_listing(source, name);

    std::vector<isa::Word> words;
    for (const assembler::ListingLine& line : new_listing) {
        if (line.has_instruction) {
            words.push_back(line.word);
        }
    }
    if (words.size() * isa::WORD_BYTES > machine::INSTRUCTION_END - machine::INSTRUCTION_BASE) {
        throw std::runtime_error("the program does not fit in the instruction region");
    }

    program_name = name;
    program_source = source;
    listing = new_listing;
    program = words;
    restart();
}

void FourStageSession::restart() {
    memory.reset();
    for (unsigned i = 0; i < program.size(); i++) {
        memory.write_instruction_word(machine::INSTRUCTION_BASE + i * isa::WORD_BYTES, program[i]);
    }
    cpu.reset();
    beat_open = false;
    beat_log.clear();
    return_slots.clear();
}

void FourStageSession::reset() {
    restart();
}

// The start of a beat: remember what each stage holds
void FourStageSession::open_beat() {
    current_beat = FourStageBeatRecord{};
    current_beat.number = beat_log.size() + 1;

    // The st that writes memory in MW this beat: remember whether it saves ra
    isa::Word store_address = 0;
    bool from_ra = false;
    if (store_of(cpu.datapath.packet_of(Stage::MW), store_address, from_ra)) {
        if (from_ra) {
            return_slots.insert(store_address);
        } else {
            return_slots.erase(store_address);
        }
    }
    for (unsigned s = unsigned(Stage::OF); s < NUM_STAGES; s++) {
        const InstructionPacket& p = cpu.datapath.packet_of(Stage(s));
        BeatCell& cell = current_beat.cells[s];
        cell.empty = p.bubble;
        cell.id = p.id;
        cell.pc = p.currentPc;
        cell.ir = p.ir;
        cell.exception = p.hasException;
    }
    beat_start_microcycle = cpu.microcycles;
    beat_fetch_pc = cpu.datapath.fetch.pc;
    beat_fetch_count = cpu.datapath.fetch_count;
    beat_taken = cpu.taken_branches;
    beat_open = true;
}

// The end of a beat (or the microcycle that halted): add it to the beat log
void FourStageSession::close_beat() {
    beat_open = false;
    if (cpu.microcycles == beat_start_microcycle) {
        return; // an exception was reported before the beat started
    }
    current_beat.microcycles = cpu.microcycles - beat_start_microcycle;

    BeatCell& fetch = current_beat.cells[unsigned(Stage::IF)];
    if (cpu.fetching) {
        fetch.empty = false;
        fetch.pc = beat_fetch_pc;
        if (cpu.datapath.fetch_count > beat_fetch_count) {
            fetch.id = cpu.datapath.fetch_count;
            fetch.ir = memory.read_word(beat_fetch_pc);
        } else {
            fetch.exception = true; // the fetch itself raised the exception
        }
    }
    if (cpu.stall) {
        for (unsigned s = 0; s <= unsigned(cpu.stall_stage); s++) {
            current_beat.cells[s].waited = true;
        }
    }
    if (cpu.taken_branches != beat_taken) {
        current_beat.taken = true;
        current_beat.target = cpu.datapath.fetch.pc;
    }
    beat_log.push_back(current_beat);
}

void FourStageSession::step_microcycle() {
    if (cpu.status != CpuStatus::Running) {
        return;
    }
    if (!beat_open) {
        open_beat();
    }
    std::uint64_t beats_before = cpu.beats;
    cpu.step();
    if (cpu.beats != beats_before || cpu.status != CpuStatus::Running) {
        close_beat();
    }
}

void FourStageSession::step_beat() {
    step_microcycle();
    while (beat_open && cpu.status == CpuStatus::Running) {
        step_microcycle();
    }
}

void FourStageSession::run(std::uint64_t max_microcycles) {
    std::uint64_t limit = cpu.microcycles + max_microcycles;
    while (cpu.status == CpuStatus::Running && cpu.microcycles < limit) {
        step_microcycle();
    }
}

json FourStageSession::program_json() const {
    json j;
    j["processor"] = "four";
    j["name"] = program_name;
    j["source"] = program_source;

    j["listing"] = json::array();
    for (const assembler::ListingLine& line : listing) {
        json l;
        l["line"] = line.line;
        l["text"] = line.text;
        l["hasInstruction"] = line.has_instruction;
        l["address"] = line.address;
        l["word"] = line.word;
        l["macro"] = line.macro;
        l["assembly"] = "";
        if (line.has_instruction) {
            l["assembly"] = disassemble(line.address, line.word);
        }
        j["listing"].push_back(l);
    }

    j["microcode"]["path"] = microcode_path;
    j["microcode"]["lines"] = microcode_lines;

    j["stages"] = json::array();
    for (unsigned s = 0; s < NUM_STAGES; s++) {
        const microassembler::StageMicroprogram& stage = stage_programs[s];
        json st;
        st["name"] = STAGE_NAMES[s];
        st["words"] = json::array();
        for (unsigned i = 0; i < stage.words.size(); i++) {
            json w = microinstruction_json(stage.words[i]);
            w["address"] = i;
            w["word"] = stage.words[i];
            w["sourceLine"] = stage.source_lines[i];
            w["part"] = bool(stage.parts[i]);
            w["text"] = "(end of section)";
            unsigned line = stage.source_lines[i];
            if (line > 0 && line <= microcode_lines.size()) {
                w["text"] = trim(microcode_lines[line - 1]);
            }
            st["words"].push_back(w);
        }
        st["entries"] = json::array();
        for (const microassembler::Entry& entry : stage.entries) {
            json e;
            e["opcode"] = unsigned(entry.opcode);
            e["address"] = entry.address;
            for (const isa::InstructionInfo& info : isa::INSTRUCTIONS) {
                if (info.opcode == entry.opcode) {
                    e["mnemonic"] = std::string(info.mnemonic);
                }
            }
            st["entries"].push_back(e);
        }
        j["stages"].push_back(st);
    }
    return j;
}

json FourStageSession::state_json() const {
    json j;
    j["processor"] = "four";
    j["status"] = status_name(cpu.status);
    j["exception"] = nullptr;
    if (cpu.status == CpuStatus::Exception) {
        j["exception"]["kind"] = exception_name(cpu.exception);
        j["exception"]["pc"] = cpu.exception_pc;
    }

    j["counters"]["microcycles"] = cpu.microcycles;
    j["counters"]["beats"] = cpu.beats;
    j["counters"]["instructions"] = cpu.instructions;
    j["counters"]["stallBeats"] = cpu.stall_beats;
    j["counters"]["takenBranches"] = cpu.taken_branches;

    j["beat"]["inProgress"] = beat_open;
    j["beat"]["number"] = beat_log.size() + 1;
    j["beat"]["microcycle"] = 0;
    if (beat_open) {
        j["beat"]["microcycle"] = cpu.microcycles - beat_start_microcycle;
    }
    j["beat"]["stall"] = cpu.stall;
    j["beat"]["stallStage"] = STAGE_NAMES[unsigned(cpu.stall_stage)];
    j["beat"]["fetching"] = cpu.fetching;
    j["beat"]["branchTaken"] = cpu.datapath.branchTaken;
    j["beat"]["branchPc"] = cpu.datapath.branchPc;

    j["stages"] = json::array();
    for (unsigned s = 0; s < NUM_STAGES; s++) {
        const four_stage_micro::StageMicroControlUnit& stage = cpu.stages[s];
        const microassembler::StageMicroprogram& program = stage_programs[s];
        json st;
        st["name"] = STAGE_NAMES[s];
        st["upc"] = stage.upc;
        st["done"] = stage.done;
        st["halted"] = stage.halted;
        st["ran"] = stage.ran;
        st["lastUpc"] = stage.last_upc;
        st["sourceLine"] = 0;
        st["part"] = false;
        if (stage.last_upc < program.source_lines.size()) {
            st["sourceLine"] = program.source_lines[stage.last_upc];
            st["part"] = bool(program.parts[stage.last_upc]);
        }
        st["signals"] = signals_json(stage.signals);
        st["writeBus"] = cpu.datapath.write_bus[s];
        st["readBus"] = cpu.datapath.read_bus[s];
        st["isMBranch"] = bool(cpu.datapath.isMBranch[s]);
        j["stages"].push_back(st);
    }

    j["latches"]["fetched"] = packet_json(cpu.datapath.fetched);
    j["latches"]["ifOf"] = packet_json(cpu.datapath.if_of);
    j["latches"]["ofEx"] = packet_json(cpu.datapath.of_ex);
    j["latches"]["exMw"] = packet_json(cpu.datapath.ex_mw);

    const four_stage_micro::Datapath& d = cpu.datapath;
    j["units"]["fetch"]["pc"] = d.fetch.pc;
    j["units"]["fetch"]["ir"] = d.fetch.ir;
    j["units"]["decode"]["I"] = d.decode.I;
    j["units"]["decode"]["rd"] = d.decode.rd;
    j["units"]["decode"]["rs1"] = d.decode.rs1;
    j["units"]["decode"]["rs2"] = d.decode.rs2;
    j["units"]["decode"]["immx"] = d.decode.immx;
    j["units"]["decode"]["branchTarget"] = d.decode.branchTarget;
    j["units"]["registerFile"]["readSrc1"] = d.register_file.readSrc1;
    j["units"]["registerFile"]["regVal1"] = d.register_file.regVal1;
    j["units"]["registerFile"]["readSrc2"] = d.register_file.readSrc2;
    j["units"]["registerFile"]["regVal2"] = d.register_file.regVal2;
    j["units"]["registerFile"]["writeSrc"] = d.register_file.writeSrc;
    j["units"]["registerFile"]["regData"] = d.register_file.regData;
    j["units"]["alu"]["A"] = d.alu.A;
    j["units"]["alu"]["B"] = d.alu.B;
    j["units"]["alu"]["aluResult"] = d.alu.aluResult;
    j["units"]["alu"]["flagE"] = d.alu.flagE;
    j["units"]["alu"]["flagGT"] = d.alu.flagGT;
    j["units"]["memoryUnit"]["mar"] = d.memory_unit.mar;
    j["units"]["memoryUnit"]["mdr"] = d.memory_unit.mdr;
    j["units"]["memoryUnit"]["ldResult"] = d.memory_unit.ldResult;

    j["registers"] = json::array();
    for (unsigned r = 0; r < isa::NUM_REGS; r++) {
        j["registers"].push_back(d.register_file.value(r));
    }

    // Data and stack memory: only the words that are not 0
    j["memory"] = json::array();
    for (isa::Word address = machine::DATA_BASE; address < machine::MEM_SIZE; address += isa::WORD_BYTES) {
        isa::Word value = memory.read_word(address);
        if (value != 0) {
            json m;
            m["address"] = address;
            m["value"] = value;
            j["memory"].push_back(m);
        }
    }

    j["stack"] = stack_json(memory, d.register_file.value(isa::REG_SP), return_slots);

    j["beatLog"] = json::array();
    for (const FourStageBeatRecord& beat : beat_log) {
        j["beatLog"].push_back(beat_json(beat));
    }
    return j;
}
}
