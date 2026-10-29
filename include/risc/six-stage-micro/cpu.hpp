#pragma once

#include <array>
#include <cstdint>

#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/microassembler/microassembler.hpp"
#include "risc/microassembler/splitter.hpp"
#include "risc/six-stage-micro/control/interlock_unit.hpp"
#include "risc/six-stage-micro/control/stage_micro_control_unit.hpp"
#include "risc/six-stage-micro/datapath/datapath.hpp"
#include "risc/six-stage-micro/pipeline.hpp"

namespace risc::six_stage_micro {

enum class CpuStatus : std::uint8_t { Running, Halted, Exception };

// The six-stage pipelined microprogrammed processor.
// The beat controller: starts every stage's section, runs microcycles until all stages are done,
// then moves every instruction one stage on.
class CPU {
private:
    memory::Memory& memory;
    bool in_beat = false; // a beat has started and not finished yet
    bool saved_flagE = false; // the flags at the start of the beat
    bool saved_flagGT = false;

    bool all_done() const {
        for (const StageMicroControlUnit& stage : stages) {
            if (!stage.done) {
                return false;
            }
        }
        return true;
    }

    // A packet that holds no instruction to run: a bubble, or an instruction that raised an exception
    static bool inactive(const InstructionPacket& p) {
        return p.bubble || p.hasException;
    }

    static bool is_hlt(const InstructionPacket& p) {
        unsigned opcode = (p.ir >> isa::OPCODE_POS) & ((1u << isa::OPCODE_BITS) - 1);
        return !inactive(p) && isa::Opcode(opcode) == isa::Opcode::HLT;
    }

    // IF stops fetching while a hlt or an instruction with an exception is in the pipeline:
    // nothing after it may run
    bool fetch_stopped() {
        for (unsigned s = unsigned(Stage::OF1); s < NUM_STAGES; s++) {
            const InstructionPacket& p = datapath.packet_of(Stage(s));
            if (is_hlt(p) || (!p.bubble && p.hasException)) {
                return true;
            }
        }
        return false;
    }

    // True if the instruction in reader (OF1 or OF2) reads a register that an older instruction,
    // further down the pipeline, has not written yet (it writes in RW)
    bool data_hazard(Stage reader) {
        const InstructionPacket& p = datapath.packet_of(reader);
        if (inactive(p)) {
            return false;
        }
        for (unsigned s = unsigned(reader) + 1; s < NUM_STAGES; s++) {
            const InstructionPacket& older = datapath.packet_of(Stage(s));
            if (inactive(older)) {
                continue;
            }
            int reg = InterlockUnit::written_register(older.ir);
            if (reg < 0) {
                continue;
            }
            if (reader == Stage::OF1 && InterlockUnit::reads_in_of1(p.ir, unsigned(reg))) {
                return true;
            }
            if (reader == Stage::OF2 && InterlockUnit::reads_in_of2(p.ir, unsigned(reg))) {
                return true;
            }
        }
        return false;
    }

    // A stage raised an exception: it is kept in the instruction's packet, and the rest of the
    // stage's section does not run. It is reported when the instruction reaches RW.
    void record_exception(StageMicroControlUnit& stage, const CpuException& e) {
        InstructionPacket& p = datapath.packet_of(stage.stage);
        if (stage.stage == Stage::IF) {
            p.bubble = false; // the instruction that could not be fetched
            p.currentPc = datapath.fetch.pc;
        }
        p.hasException = true;
        p.exception = e;
        stage.done = true;
    }

    // Start of a beat: check for a data hazard, then every stage that is not waiting
    // jumps to its section for the instruction in its input latch
    void start_beat() {
        datapath.branchTaken = false;
        saved_flagE = datapath.alu.flagE;
        saved_flagGT = datapath.alu.flagGT;

        stall = false;
        if (data_hazard(Stage::OF2)) {
            stall = true;
            stall_stage = Stage::OF2;
        } else if (data_hazard(Stage::OF1)) {
            stall = true;
            stall_stage = Stage::OF1;
        }
        if (stall) {
            stall_beats++;
        }
        fetching = !stall && !fetch_stopped();

        for (StageMicroControlUnit& stage : stages) {
            const InstructionPacket& p = datapath.packet_of(stage.stage);
            if (stall && stage.stage <= stall_stage) {
                stage.done = true; // waits this beat
            } else if (stage.stage == Stage::IF && !fetching) {
                stage.done = true;
            } else if (stage.stage != Stage::IF && p.hasException) {
                stage.done = true; // an instruction with an exception does nothing more
            } else {
                try {
                    stage.start(p);
                } catch (const CpuException& e) {
                    record_exception(stage, e); // e.g. no microcode for the opcode
                }
            }
        }
        in_beat = true;
    }

    // End of a beat: the latches clock and every instruction moves one stage on.
    // In a stall, the waiting instructions stay where they are and a bubble goes in after them.
    // After a taken branch, IF continues at the branch's target and the three younger instructions are flushed.
    // The instructions behind one with an exception are flushed.
    void end_beat() {
        if (datapath.ex_ma.hasException) {
            // MA raised it: undo the flags of the younger instruction that ran in EX this beat
            datapath.alu.flagE = saved_flagE;
            datapath.alu.flagGT = saved_flagGT;
        }

        if (!datapath.ma_rw.bubble) {
            instructions++; // the instruction in RW has finished
        }
        datapath.ma_rw = datapath.ex_ma;
        datapath.ex_ma = datapath.of2_ex;
        if (stall && stall_stage == Stage::OF2) {
            datapath.of2_ex = InstructionPacket{};
        } else {
            datapath.of2_ex = datapath.of1_of2;
            if (stall && stall_stage == Stage::OF1) {
                datapath.of1_of2 = InstructionPacket{};
            } else {
                datapath.of1_of2 = datapath.if_of1;
                datapath.if_of1 = datapath.fetched;
            }
        }
        datapath.fetched = InstructionPacket{};

        if (datapath.branchTaken) {
            datapath.fetch.pc = datapath.branchPc;
            datapath.if_of1 = InstructionPacket{};
            datapath.of1_of2 = InstructionPacket{};
            datapath.of2_ex = InstructionPacket{};
            taken_branches++;
        }

        // Latches from the youngest instruction to the oldest
        std::array<InstructionPacket*, 5> latches{&datapath.if_of1, &datapath.of1_of2, &datapath.of2_ex,
                                                   &datapath.ex_ma, &datapath.ma_rw};
        for (int i = 4; i >= 0; i--) {
            if (!latches[i]->bubble && latches[i]->hasException) {
                for (int j = 0; j < i; j++) {
                    *latches[j] = InstructionPacket{};
                }
                break;
            }
        }

        beats++;
        in_beat = false;
    }

public:
    Datapath datapath;
    std::array<StageMicroControlUnit, NUM_STAGES> stages{
        StageMicroControlUnit(Stage::IF), StageMicroControlUnit(Stage::OF1), StageMicroControlUnit(Stage::OF2),
        StageMicroControlUnit(Stage::EX), StageMicroControlUnit(Stage::MA), StageMicroControlUnit(Stage::RW)};

    CpuStatus status = CpuStatus::Running;
    CpuException exception{}; // valid when status == Exception
    isa::Word exception_pc = 0; // address of the instruction that caused it
    std::uint64_t microcycles = 0;
    std::uint64_t beats = 0;
    std::uint64_t instructions = 0; // instructions that have finished RW
    std::uint64_t stall_beats = 0; // beats in which a data hazard made IF..OF1 or IF..OF2 wait
    std::uint64_t taken_branches = 0; // each one flushes the three instructions behind it

    // The current (or last) beat, for display
    bool stall = false; // IF up to stall_stage wait
    Stage stall_stage = Stage::IF;
    bool fetching = false; // IF fetches an instruction

    CPU(memory::Memory& memory) : memory(memory) {
        reset();
    }

    // Leaves memory and the stage microprograms alone
    void reset() {
        datapath.reset();
        for (StageMicroControlUnit& stage : stages) {
            stage.reset();
        }
        in_beat = false;
        stall = false;
        fetching = false;
        status = CpuStatus::Running;
        exception = CpuException{};
        exception_pc = 0;
        microcycles = 0;
        beats = 0;
        instructions = 0;
        stall_beats = 0;
        taken_branches = 0;
    }

    // Store each stage's microprogram and switch table (from microassembler::split)
    void write_stage_microprograms(const std::array<microassembler::StageMicroprogram, NUM_STAGES>& programs) {
        for (unsigned s = 0; s < NUM_STAGES; s++) {
            stages[s].write_microprogram_words(programs[s].words);
            for (const microassembler::Entry& entry : programs[s].entries) {
                stages[s].switch_unit.set_start(entry.opcode, entry.address);
            }
        }
    }

    // One microcycle. A beat starts on the first microcycle after the previous beat ended.
    void step() {
        if (status != CpuStatus::Running) {
            return;
        }
        if (!in_beat) {
            if (datapath.ma_rw.hasException) {
                // Every older instruction has finished and every younger one was flushed (precise)
                status = CpuStatus::Exception;
                exception = datapath.ma_rw.exception;
                exception_pc = datapath.ma_rw.currentPc;
                return;
            }
            start_beat();
        }

        microcycles++;
        for (StageMicroControlUnit& stage : stages) {
            try {
                stage.step(datapath, memory);
            } catch (const CpuException& e) {
                record_exception(stage, e);
            }
        }
        if (stages[unsigned(Stage::RW)].halted) {
            instructions++; // the hlt
            status = CpuStatus::Halted;
            return;
        }
        if (all_done()) {
            end_beat();
        }
    }

    // Run until the processor halts or raises an exception, or max_microcycles have passed
    void run(std::uint64_t max_microcycles = 1'000'000) {
        while (status == CpuStatus::Running && microcycles < max_microcycles) {
            step();
        }
    }
};
}
