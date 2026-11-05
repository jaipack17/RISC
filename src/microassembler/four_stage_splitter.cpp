#include "risc/microassembler/four_stage_splitter.hpp"

#include <string>
#include <vector>

#include "risc/four-stage-micro/pipeline.hpp"
#include "risc/microassembler/microassembler.hpp"
#include "risc/micro/control/micro_decode_unit.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/micro/micro_isa.hpp"

namespace risc::microassembler {

using four_stage_micro::Stage;
using micro::MicroReg;

namespace {

// The stage that does the microinstruction's own work: mloadIR, mdecode, the bus move or the mbeq test
Stage work_stage(const micro::Microinstruction& m, unsigned line) {
    if (m.type == micro::MicroOp::mloadIR) {
        return Stage::IF;
    }
    if (m.type == micro::MicroOp::mdecode) {
        return Stage::OF;
    }
    if (m.args != micro::NO_ARGS && micro::args_unit(m.args) == micro::UnitId::Control) {
        return Stage::MW; // hlt's line runs in MW
    }

    if (m.type == micro::MicroOp::mbeq) {
        if (m.src == MicroReg::I || m.src == MicroReg::flagE || m.src == MicroReg::flagGT) {
            return Stage::EX;
        }
    }

    if (m.type == micro::MicroOp::mmov || m.type == micro::MicroOp::mmovi || m.type == micro::MicroOp::madd) {
        if (m.dest == MicroReg::pc) {
            if (m.type == micro::MicroOp::madd) {
                return Stage::IF; // madd pc, 4
            }
            return Stage::EX; // a branch
        }
        if (m.dest == MicroReg::A) {
            return Stage::OF;
        }
        if (m.dest == MicroReg::B) {
            return Stage::EX;
        }
        if (m.dest == MicroReg::regSrc) {
            bool is_read = m.args != micro::NO_ARGS && micro::args_unit(m.args) == micro::UnitId::RegisterFile &&
                           micro::RegisterFileOp(micro::args_opcode(m.args)) == micro::RegisterFileOp::Read;
            if (is_read && m.type == micro::MicroOp::mmov && m.src == MicroReg::rs1) {
                return Stage::OF; // the first operand
            }
            if (is_read) {
                return Stage::EX; // rs2, rd (st) or r15 (ret)
            }
            return Stage::MW;
        }
        if (m.dest == MicroReg::regData) {
            if (m.src == MicroReg::regVal || m.src == MicroReg::immx) {
                return Stage::EX; // mov
            }
            return Stage::MW;
        }
        if (m.dest == MicroReg::mar || m.dest == MicroReg::mdr) {
            return Stage::MW;
        }
    }

    throw MicroassemblerError(line, "cannot tell which stage this microinstruction belongs to");
}

// The stage whose unit runs the args
Stage args_stage(const micro::Microinstruction& m, unsigned line) {
    micro::UnitId unit = micro::args_unit(m.args);
    if (unit == micro::UnitId::Alu) {
        return Stage::EX;
    }
    if (unit == micro::UnitId::Memory) {
        return Stage::MW;
    }
    if (unit == micro::UnitId::RegisterFile) {
        if (micro::RegisterFileOp(micro::args_opcode(m.args)) == micro::RegisterFileOp::Read) {
            return work_stage(m, line); // the read side of the stage that set regSrc
        }
        return Stage::MW;
    }
    return Stage::MW; // <halt>
}

// One microinstruction of a stage's section
struct SectionLine {
    micro::Microinstruction m;
    unsigned source_line = 0; // 0 for the end marker
    bool part = false; // only part of the source line (a mixed line was split)
};

// A stage's section for one instruction. mbeq targets are indexes into the section.
using Section = std::vector<SectionLine>;

// The end of a section: an mswitch (see StageMicroControlUnit::at_end)
Section end_of_section() {
    Section section;
    section.push_back(SectionLine{micro::mswitch(), 0, false});
    return section;
}

// Move a section `offset` lines further down: its mbeq targets move with it
void shift_targets(Section& section, unsigned offset) {
    for (SectionLine& line : section) {
        if (line.m.type == micro::MicroOp::mbeq) {
            line.m.branchTarget += offset;
        }
    }
}

// True if two sections run the same microinstructions (source lines are not compared)
bool same_code(const Section& a, const Section& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (unsigned i = 0; i < a.size(); i++) {
        if (micro::encode(a[i].m) != micro::encode(b[i].m)) {
            return false;
        }
    }
    return true;
}

// The part of microinstruction m that runs in `stage`. Returns false if no part of it does.
bool part_for_stage(const micro::Microinstruction& m, unsigned line, Stage stage, SectionLine& part) {
    Stage work = work_stage(m, line);
    Stage args = work;
    if (m.args != micro::NO_ARGS) {
        args = args_stage(m, line);
    }

    if (work == stage && args == stage) {
        part = SectionLine{m, line, false};
        return true;
    }
    if (work == stage) {
        part = SectionLine{m, line, true};
        part.m.args = micro::NO_ARGS; // the args run in another stage
        return true;
    }
    if (args == stage) {
        part = SectionLine{micro::mmov(MicroReg::None, MicroReg::None, m.args), line, true}; // args only
        return true;
    }
    return false;
}

// Stage `stage`'s section of the microcode, from µaddress `address` to the end of the routine.
// The preamble starts at µaddress 0. Its mswitch continues at `entry`, where the instruction's routine starts.
Section build_section(const Microprogram& program, unsigned address, unsigned entry, Stage stage,
                      unsigned depth) {
    if (depth > micro::MICROPROGRAM_SIZE) {
        throw MicroassemblerError(0, "a routine never jumps back to the preamble");
    }

    micro::Microinstruction m = micro::MicroDecodeUnit::fields(program.words[address]);
    unsigned line = program.source_lines[address];

    if (m.type == micro::MicroOp::mswitch) {
        if (stage == Stage::IF) {
            return end_of_section(); // IF only runs the preamble
        }
        return build_section(program, entry, entry, stage, depth + 1);
    }

    if (m.type == micro::MicroOp::mb) {
        if (m.branchTarget == 0) {
            return end_of_section(); // mb .begin: the routine is over
        }
        return build_section(program, m.branchTarget, entry, stage, depth + 1);
    }

    if (m.type == micro::MicroOp::mbeq) {
        Section not_taken = build_section(program, address + 1, entry, stage, depth + 1);
        Section taken = build_section(program, m.branchTarget, entry, stage, depth + 1);
        if (same_code(not_taken, taken)) {
            return not_taken; // the test does not matter to this stage
        }
        if (stage < work_stage(m, line)) {
            throw MicroassemblerError(line, "an earlier stage depends on this test");
        }

        // mbeq, then the not-taken path, then the taken path
        Section section;
        SectionLine test{m, line, false};
        test.m.branchTarget = 1 + not_taken.size();
        section.push_back(test);
        shift_targets(not_taken, 1);
        shift_targets(taken, 1 + not_taken.size());
        section.insert(section.end(), not_taken.begin(), not_taken.end());
        section.insert(section.end(), taken.begin(), taken.end());
        return section;
    }

    // mloadIR, mdecode, mmov, mmovi or madd
    Section rest;
    if (m.args != micro::NO_ARGS && micro::args_unit(m.args) == micro::UnitId::Control) {
        rest = end_of_section(); // nothing runs after <halt>
    } else {
        rest = build_section(program, address + 1, entry, stage, depth + 1);
    }

    SectionLine part;
    if (!part_for_stage(m, line, stage, part)) {
        return rest;
    }
    shift_targets(rest, 1);
    rest.insert(rest.begin(), part);
    return rest;
}

// Put a section at the end of a stage's microprogram. Returns the µaddress where it starts.
unsigned append_section(StageMicroprogram& stage, const Section& section) {
    unsigned start = stage.words.size();
    for (const SectionLine& line : section) {
        micro::Microinstruction m = line.m;
        if (m.type == micro::MicroOp::mbeq) {
            m.branchTarget += start;
        }
        stage.words.push_back(micro::encode(m));
        stage.source_lines.push_back(line.source_line);
        stage.parts.push_back(line.part);
    }
    return start;
}

} // namespace

std::array<StageMicroprogram, four_stage_micro::NUM_STAGES> split_four(const Microprogram& program) {
    if (program.words.empty()) {
        throw MicroassemblerError(0, "the microprogram is empty");
    }

    std::array<StageMicroprogram, four_stage_micro::NUM_STAGES> stages;

    // IF runs the same section for every instruction: its part of the preamble
    append_section(stages[unsigned(Stage::IF)], build_section(program, 0, 0, Stage::IF, 0));

    for (unsigned s = unsigned(Stage::OF); s < four_stage_micro::NUM_STAGES; s++) {
        StageMicroprogram& stage = stages[s];
        append_section(stage, end_of_section()); // µaddress 0: the empty section, shared by all instructions

        for (const Entry& entry : program.entries) {
            Section section = build_section(program, 0, entry.address, Stage(s), 0);
            unsigned start = 0;
            if (section.size() > 1) { // more than just the end marker
                start = append_section(stage, section);
            }
            stage.entries.push_back(Entry{entry.opcode, start});
        }

        if (stage.words.size() > micro::MICROPROGRAM_SIZE) {
            throw MicroassemblerError(0, "a stage's microprogram is longer than " +
                                             std::to_string(micro::MICROPROGRAM_SIZE) + " microinstructions");
        }
    }
    return stages;
}
}
