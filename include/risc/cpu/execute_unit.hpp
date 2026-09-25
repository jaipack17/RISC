#pragma once

#include "risc/cpu/alu.hpp"
#include "risc/cpu/flags.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/cpu/operand_fetch_unit.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::cpu {

struct ExecuteOutput {
    isa::Word aluResult = 0;
    bool isBranchTaken = false;
    isa::Word branchPC = 0;
};

class ExecuteUnit {
private:
    ALU alu;

public:
    Flags flags;

    void reset() {
        flags = Flags{};
    }

    ExecuteOutput run(const OperandFetchOutput& in, const ControlSignals& ctrl) {
        ExecuteOutput out;

        // Branch unit: multiplexer picks op1 (ra) for ret, else branchTarget
        if (ctrl.isRet) {
            out.branchPC = in.op1;
        } else {
            out.branchPC = in.branchTarget;
        }

        bool beq_taken = ctrl.isBeq && flags.is_set(isa::FLAG_E);
        bool bgt_taken = ctrl.isBgt && flags.is_set(isa::FLAG_GT);
        bool bc_taken = ctrl.isBc && flags.is_set(isa::FLAG_C);
        bool bv_taken = ctrl.isBv && flags.is_set(isa::FLAG_V);
        out.isBranchTaken = ctrl.isUBranch || beq_taken || bgt_taken || bc_taken || bv_taken;

        // ALU unit: multiplexer picks immx or op2
        isa::Word b;
        if (ctrl.isImmediate) {
            b = in.immx;
        } else {
            b = in.op2;
        }
        out.aluResult = alu.execute(in.op1, b, ctrl, flags);
        return out;
    }
};
}
