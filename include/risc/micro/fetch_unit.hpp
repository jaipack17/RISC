#pragma once

#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"

namespace risc::micro {

class FetchUnit {
public:
    isa::Word pc = machine::RESET_PC; // program counter
    isa::Word ir = 0; // instruction register

    void reset() {
        pc = machine::RESET_PC;
        ir = 0;
    }

    isa::Word fetch_address() const {
        if (pc % isa::WORD_BYTES != 0) {
            throw CpuException{isa::Exception::MisalignedFetch, pc};
        }
        if (pc < machine::INSTRUCTION_BASE || pc >= machine::INSTRUCTION_END) {
            throw CpuException{isa::Exception::ExecuteOutsideText, pc};
        }
        return pc;
    }

    void load_ir(isa::Word instruction) {
        ir = instruction;
    }
};

}
