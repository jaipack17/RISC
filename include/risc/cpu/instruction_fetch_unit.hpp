#pragma once

#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"

namespace risc::cpu {

class InstructionFetchUnit {
private:
    isa::Word pc = machine::RESET_PC;

public:
    void reset() {
        pc = machine::RESET_PC;
    }

    // The address sent to instruction memory (throws if pc is not a valid instruction address)
    isa::Word fetch_address() const {
        if (pc % isa::WORD_BYTES != 0) {
            throw CpuException{isa::Exception::MisalignedFetch, pc};
        }
        if (pc < machine::TEXT_BASE || pc >= machine::TEXT_END) {
            throw CpuException{isa::Exception::ExecuteOutsideText, pc};
        }
        return pc;
    }

    // Multiplexer + pc register. branchPC if the branch is taken, else pc + 4
    void update_pc(bool isBranchTaken, isa::Word branchPC) {
        if (isBranchTaken) {
            pc = branchPC;
        } else {
            pc = pc + isa::WORD_BYTES;
        }
    }
};
}
