#include "session.hpp"

#include <fstream>
#include <stdexcept>

#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/micro/control/micro_decode_unit.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/micro/micro_isa.hpp"
#include "risc/visualizer/visualizer.hpp"

namespace risc::web {

using six_stage_micro::CpuStatus;
using six_stage_micro::InstructionPacket;
using six_stage_micro::NUM_STAGES;
using six_stage_micro::Stage;

namespace {

const char* STAGE_NAMES[] = {"IF", "OF1", "OF2", "EX", "MA", "RW"};

// In the same order as micro::MicroReg
const char* REGISTER_NAMES[] = {"",        "pc",     "ir",     "I",     "rd",    "rs1",    "rs2",
                                "immx",    "branchTarget", "regSrc", "regData", "regVal", "A", "B",
                                "flagE",   "flagGT", "aluResult", "mar", "mdr",   "ldResult"};

// In the same order as micro::MicroOp
const char* MICRO_OP_NAMES[] = {"mloadIR", "mdecode", "mswitch", "mmov", "mmovi", "madd", "mbeq", "mb"};

// In the same order as micro::AluOp
const char* ALU_OP_NAMES[] = {"nop", "add", "sub", "cmp", "mul", "div", "mod",
                              "lsl", "lsr", "asr", "and", "or",  "xor", "not"};

// In the same order as micro::TransferMux and micro::MicroPCMux
const char* TRANSFER_MUX_NAMES[] = {"writeBus", "immediate", "microAdder"};
const char* MICRO_PC_MUX_NAMES[] = {"next", "branchTarget", "switch", "branchIfMBranch"};

// In the same order as isa::Exception
const char* EXCEPTION_NAMES[] = {"IllegalInstruction", "MisalignedAccess", "MisalignedFetch",
                                 "OutOfBoundsAccess", "WriteToText", "ExecuteOutsideText",
                                 "DivideByZero", "StackOverflow", "StackUnderflow"};

std::string register_name(micro::MicroReg reg) {
    unsigned index = unsigned(reg);
    if (index < std::size(REGISTER_NAMES)) {
        return REGISTER_NAMES[index];
    }
    return "?";
}

// The text of an args field, e.g. "add", "read", "load", "halt" ("" for no args)
std::string args_name(micro::Args a) {
    if (a == micro::NO_ARGS) {
        return "";
    }
    unsigned opcode = micro::args_opcode(a);
    micro::UnitId unit = micro::args_unit(a);
    if (unit == micro::UnitId::Alu && opcode < std::size(ALU_OP_NAMES)) {
        return ALU_OP_NAMES[opcode];
    }
    if (unit == micro::UnitId::RegisterFile) {
        if (micro::RegisterFileOp(opcode) == micro::RegisterFileOp::Read) {
            return "read";
        }
        return "write";
    }
    if (unit == micro::UnitId::Memory) {
        if (micro::MemoryOp(opcode) == micro::MemoryOp::Load) {
            return "load";
        }
        return "store";
    }
    if (unit == micro::UnitId::Control) {
        return "halt";
    }
    return "?";
}

// The unit an args field goes to
std::string args_unit_name(micro::Args a) {
    if (a == micro::NO_ARGS) {
        return "";
    }
    micro::UnitId unit = micro::args_unit(a);
    if (unit == micro::UnitId::Alu) {
        return "alu";
    }
    if (unit == micro::UnitId::RegisterFile) {
        return "registerFile";
    }
    if (unit == micro::UnitId::Memory) {
        return "memory";
    }
    return "control";
}

std::string trim(const std::string& text) {
    std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return "";
    }
    std::size_t last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::string disassemble(isa::Word pc, isa::Word ir) {
    return visualizer::disassemble(pc, ir);
}

json microinstruction_json(micro::MicroWord word) {
    micro::Microinstruction m = micro::MicroDecodeUnit::fields(word);
    json j;
    j["type"] = MICRO_OP_NAMES[unsigned(m.type)];
    j["src"] = register_name(m.src);
    j["dest"] = register_name(m.dest);
    j["imm"] = m.imm;
    j["branchTarget"] = m.branchTarget;
    j["args"] = args_name(m.args);
    j["argsUnit"] = args_unit_name(m.args);
    return j;
}

json packet_json(const InstructionPacket& p) {
    json j;
    j["bubble"] = p.bubble;
    j["id"] = p.id;
    j["currentPc"] = p.currentPc;
    j["nextPc"] = p.nextPc;
    j["ir"] = p.ir;
    j["assembly"] = "";
    if (!p.bubble && !p.hasException) {
        j["assembly"] = disassemble(p.currentPc, p.ir);
    }
    j["I"] = p.I;
    j["rd"] = p.rd;
    j["rs1"] = p.rs1;
    j["rs2"] = p.rs2;
    j["immx"] = p.immx;
    j["branchTarget"] = p.branchTarget;
    j["A"] = p.A;
    j["B"] = p.B;
    j["regVal"] = p.regVal;
    j["regData"] = p.regData;
    j["aluResult"] = p.aluResult;
    j["ldResult"] = p.ldResult;
    j["hasException"] = p.hasException;
    j["exception"] = "";
    if (p.hasException) {
        j["exception"] = EXCEPTION_NAMES[unsigned(p.exception.kind)];
    }
    return j;
}

json beat_json(const BeatRecord& beat) {
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

Session::Session(const std::string& microcode_path) : microcode_path(microcode_path) {
    microassembler::Microassembler microassembler;
    microprogram = microassembler.assemble_file(microcode_path);
    stage_programs = microassembler::split(microprogram);
    cpu.write_stage_microprograms(stage_programs);

    std::ifstream file(microcode_path);
    std::string line;
    while (std::getline(file, line)) {
        microcode_lines.push_back(line);
    }
    restart();
}

void Session::load(const std::string& name, const std::string& source) {
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

void Session::restart() {
    memory.reset();
    for (unsigned i = 0; i < program.size(); i++) {
        memory.write_instruction_word(machine::INSTRUCTION_BASE + i * isa::WORD_BYTES, program[i]);
    }
    cpu.reset();
    beat_open = false;
    beat_log.clear();
}

void Session::reset() {
    restart();
}

// The start of a beat: remember what each stage holds
void Session::open_beat() {
    current_beat = BeatRecord{};
    current_beat.number = beat_log.size() + 1;
    for (unsigned s = unsigned(Stage::OF1); s < NUM_STAGES; s++) {
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
void Session::close_beat() {
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

void Session::step_microcycle() {
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

void Session::step_beat() {
    step_microcycle();
    while (beat_open && cpu.status == CpuStatus::Running) {
        step_microcycle();
    }
}

void Session::run(std::uint64_t max_microcycles) {
    std::uint64_t limit = cpu.microcycles + max_microcycles;
    while (cpu.status == CpuStatus::Running && cpu.microcycles < limit) {
        step_microcycle();
    }
}

json Session::program_json() const {
    json j;
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

json Session::state_json() const {
    json j;
    j["status"] = status_name(cpu.status);
    j["exception"] = nullptr;
    if (cpu.status == CpuStatus::Exception) {
        j["exception"]["kind"] = EXCEPTION_NAMES[unsigned(cpu.exception.kind)];
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
        const six_stage_micro::StageMicroControlUnit& stage = cpu.stages[s];
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
        st["signals"]["loadIR"] = stage.signals.loadIR;
        st["signals"]["decode"] = stage.signals.decode;
        st["signals"]["transfer"] = stage.signals.transfer;
        st["signals"]["src"] = register_name(stage.signals.src);
        st["signals"]["dest"] = register_name(stage.signals.dest);
        st["signals"]["mux"] = TRANSFER_MUX_NAMES[unsigned(stage.signals.mux)];
        st["signals"]["imm"] = stage.signals.imm;
        st["signals"]["args"] = args_name(stage.signals.args);
        st["signals"]["argsUnit"] = args_unit_name(stage.signals.args);
        st["signals"]["next"] = MICRO_PC_MUX_NAMES[unsigned(stage.signals.next)];
        st["signals"]["branchTarget"] = stage.signals.branchTarget;
        st["writeBus"] = cpu.datapath.write_bus[s];
        st["readBus"] = cpu.datapath.read_bus[s];
        st["isMBranch"] = bool(cpu.datapath.isMBranch[s]);
        j["stages"].push_back(st);
    }

    j["latches"]["fetched"] = packet_json(cpu.datapath.fetched);
    j["latches"]["ifOf1"] = packet_json(cpu.datapath.if_of1);
    j["latches"]["of1Of2"] = packet_json(cpu.datapath.of1_of2);
    j["latches"]["of2Ex"] = packet_json(cpu.datapath.of2_ex);
    j["latches"]["exMa"] = packet_json(cpu.datapath.ex_ma);
    j["latches"]["maRw"] = packet_json(cpu.datapath.ma_rw);

    const six_stage_micro::Datapath& d = cpu.datapath;
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

    j["beatLog"] = json::array();
    for (const BeatRecord& beat : beat_log) {
        j["beatLog"].push_back(beat_json(beat));
    }
    return j;
}
}
