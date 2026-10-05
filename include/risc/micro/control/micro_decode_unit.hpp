#pragma once

#include <cstdint>

#include "risc/isa/isa_config.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/micro/datapath/datapath.hpp"
#include "risc/micro/micro_isa.hpp"

namespace risc::micro {

// µfetch multiplexer: where the next µpc comes from (Table 9.9)
enum class MicroPCMux : std::uint8_t { Next, BranchTarget, Switch, BranchIfMBranch };

// The control signals for one microinstruction
struct MicroControlSignals {
    bool loadIR = false; // fetch unit: ir <- [pc]
    bool decode = false; // decode unit: fill the decode registers
    bool transfer = false; // use the shared bus
    MicroReg src = MicroReg::None;
    MicroReg dest = MicroReg::None;
    TransferMux mux = TransferMux::WriteBus;
    isa::Word imm = 0; // µimm, sign-extended
    Args args = NO_ARGS;
    MicroPCMux next = MicroPCMux::Next;
    unsigned branchTarget = 0;
};

// Figure 9.25: splits a 45-bit microinstruction into fields, then generates its control signals
class MicroDecodeUnit {
private:
    static MicroWord field(MicroWord word, unsigned pos, unsigned width) {
        MicroWord mask = (MicroWord{1} << width) - 1;
        return (word >> pos) & mask;
    }

    static int sign_extend(MicroWord value, unsigned width) {
        unsigned shift = 32 - width;
        return isa::SWord(isa::Word(value) << shift) >> shift;
    }

public:
    // The fields of a microinstruction word
    static Microinstruction fields(MicroWord word) {
        Microinstruction m;
        m.type = MicroOp(field(word, TYPE_POS, TYPE_BITS));
        m.src = MicroReg(field(word, SRC_POS, MICRO_REG_BITS));
        m.dest = MicroReg(field(word, DEST_POS, MICRO_REG_BITS));
        m.imm = sign_extend(field(word, MICRO_IMM_POS, MICRO_IMM_BITS), MICRO_IMM_BITS);
        m.branchTarget = field(word, BRANCH_TARGET_POS, MICRO_ADDR_BITS);
        m.args = Args(field(word, ARGS_POS, ARGS_BITS));
        return m;
    }

    MicroControlSignals decode(MicroWord word) const {
        Microinstruction m = fields(word);

        MicroControlSignals s;
        s.src = m.src;
        s.dest = m.dest;
        s.imm = isa::Word(m.imm);
        s.args = m.args;
        s.branchTarget = m.branchTarget;

        switch (m.type) {
            case MicroOp::mloadIR:
                s.loadIR = true;
                break;
            case MicroOp::mdecode:
                s.decode = true;
                break;
            case MicroOp::mswitch:
                s.next = MicroPCMux::Switch;
                break;
            case MicroOp::mmov:
                s.transfer = true;
                s.mux = TransferMux::WriteBus;
                break;
            case MicroOp::mmovi:
                s.transfer = true;
                s.mux = TransferMux::Immediate;
                break;
            case MicroOp::madd:
                s.transfer = true;
                s.mux = TransferMux::MicroAdder;
                break;
            case MicroOp::mbeq:
                s.transfer = true; // puts src on the write bus to compute isMBranch
                s.next = MicroPCMux::BranchIfMBranch;
                break;
            case MicroOp::mb:
                s.next = MicroPCMux::BranchTarget;
                break;
        }
        return s;
    }
};
}
