#pragma once

#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/cpu/register_file.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::cpu {

class RegisterWritebackUnit {
public:
    void run(isa::Word pc, isa::Word inst, isa::Word aluResult, isa::Word ldResult,
             const ControlSignals& ctrl, RegisterFile& rf) const {
        // pc + 4 for call (10), ldResult for ld (01), else aluResult (00)
        isa::Word result;
        if (ctrl.isCall) {
            result = pc + isa::WORD_BYTES;
        } else if (ctrl.isLd) {
            result = ldResult;
        } else {
            result = aluResult;
        }

        // ra for call, else rd
        unsigned rd = (inst >> isa::RD_POS) & ((1u << isa::REG_BITS) - 1);
        unsigned address;
        if (ctrl.isCall) {
            address = isa::REG_RA;
        } else {
            address = rd;
        }

        rf.write_port.enable = ctrl.isWb;
        rf.write_port.address = address;
        rf.write_port.data = result;
        rf.write();
    }
};
}
