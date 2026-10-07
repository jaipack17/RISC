#pragma once

#include "risc/cpu/alu.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/cpu/operand_fetch_unit.hpp"

namespace risc::cpu::six_stage {

struct ALUStageOutput {
    isa::Word result = 0;
    isa::Word branch_target = 0;
    isa::Word return_target = 0;
    Flags flag_values{};
    std::uint8_t flags_written = 0;
};

class ALUStageUnit {
private:
    ALU alu;

public:
    ALUStageOutput run(const OperandFetchOutput& operands,
                       const ControlSignals& control) const {
        ALUStageOutput out;
        out.branch_target = operands.branchTarget;
        out.return_target = operands.op1;

        const isa::Word operand_b = control.isImmediate
            ? operands.immx
            : operands.op2;
        out.result = alu.execute(operands.op1, operand_b, control, out.flag_values);

        if (control.isCmp) {
            out.flags_written = isa::FLAG_E | isa::FLAG_GT |
                                isa::FLAG_C | isa::FLAG_V;
        } else if ((control.isAdd || control.isSub) &&
                   !control.isLd && !control.isSt) {
            out.flags_written = isa::FLAG_C | isa::FLAG_V;
        }
        return out;
    }
};

} // namespace risc::cpu::six_stage
