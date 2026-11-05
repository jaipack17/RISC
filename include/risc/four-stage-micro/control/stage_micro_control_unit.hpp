#pragma once

#include <array>
#include <vector>

#include "risc/four-stage-micro/datapath/datapath.hpp"
#include "risc/four-stage-micro/pipeline.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/control/micro_decode_unit.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/micro/control/switch_unit.hpp"

namespace risc::four_stage_micro {

// The µcontrol unit of one stage: runs that stage's section of the current instruction's microcode
class StageMicroControlUnit {
private:
    micro::MicroDecodeUnit micro_decode;

    // A section ends where the stage's microprogram has an mswitch
    bool at_end() const {
        return micro::MicroDecodeUnit::fields(microprogram[upc]).type == micro::MicroOp::mswitch;
    }

    // Execute unit: applies the control signals to this stage's part of the datapath
    void apply_signals(const micro::MicroControlSignals& s, Datapath& datapath, memory::Memory& memory) {
        if (s.loadIR) {
            datapath.load_ir(memory);
        }
        if (s.decode) {
            datapath.decode_ir();
        }
        if (s.transfer) {
            datapath.transfer(stage, s.src, s.dest, s.mux, s.imm);
        }
        if (datapath.run_args(stage, s.args, memory)) {
            halted = true;
        }
    }

    // µfetch multiplexer (mswitch never runs here, it only marks the end of a section)
    unsigned next_upc(const micro::MicroControlSignals& s, const Datapath& datapath) const {
        if (s.next == micro::MicroPCMux::BranchTarget) {
            return s.branchTarget;
        }
        if (s.next == micro::MicroPCMux::BranchIfMBranch && datapath.isMBranch[unsigned(stage)]) {
            return s.branchTarget;
        }
        return (upc + 1) % micro::MICROPROGRAM_SIZE;
    }

public:
    Stage stage;
    unsigned upc = 0; // µpc
    std::array<micro::MicroWord, micro::MICROPROGRAM_SIZE> microprogram{}; // this stage's microprogram memory
    micro::SwitchUnit switch_unit;
    micro::MicroControlSignals signals; // signals of the last microinstruction (for display)
    bool done = true; // finished its section for this beat
    bool halted = false;
    bool ran = false; // ran a microinstruction in the last microcycle (for display)
    unsigned last_upc = 0; // the µaddress it ran

    StageMicroControlUnit(Stage s) {
        stage = s;
    }

    void reset() {
        upc = 0;
        signals = micro::MicroControlSignals{};
        done = true;
        halted = false;
        ran = false;
        last_upc = 0;
    }

    // Store this stage's microprogram, starting at address 0
    void write_microprogram_words(const std::vector<micro::MicroWord>& words) {
        for (unsigned i = 0; i < words.size(); i++) {
            microprogram[i] = words[i];
        }
    }

    // Start of a beat: set µpc to the start of this stage's section for the instruction in packet
    void start(const InstructionPacket& packet) {
        if (stage == Stage::IF) {
            upc = 0; // IF has one section for all instructions
        } else if (packet.bubble) {
            done = true;
            return;
        } else {
            upc = switch_unit.get_start_address(packet.ir);
        }
        done = at_end(); // an empty section ends straight away
    }

    // One microcycle: run the microinstruction at µpc, then pick the next µpc
    void step(Datapath& datapath, memory::Memory& memory) {
        ran = false;
        if (done || halted) {
            return;
        }
        ran = true;
        last_upc = upc;
        signals = micro_decode.decode(microprogram[upc]);
        apply_signals(signals, datapath, memory);
        upc = next_upc(signals, datapath);
        done = at_end();
    }
};
}
