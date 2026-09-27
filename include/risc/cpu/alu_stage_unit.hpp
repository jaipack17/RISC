#pragma once

#include <cstdint>

#include "risc/cpu/alu.hpp"
#include "risc/cpu/flags.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/cpu/operand_fetch_unit.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::cpu {

// Values produced by the ALU stage and carried to the Branch stage.
struct ALUStageOutput {
    isa::Word result = 0;
    isa::Word branch_target = 0;
    isa::Word return_target = 0;
    Flags flag_values{};
    std::uint8_t flags_written = 0;
};

// Computes ALU results without changing architectural flags. Flag results are
// carried forward and committed by BranchStageUnit.
class ALUStageUnit {
private:
    ALU alu;

    static std::uint8_t flag_write_mask(const ControlSignals& ctrl) {
        if (ctrl.isCmp) {
            return isa::FLAG_E | isa::FLAG_GT | isa::FLAG_C | isa::FLAG_V;
        }
        if ((ctrl.isAdd || ctrl.isSub) && !ctrl.isLd && !ctrl.isSt) {
            return isa::FLAG_C | isa::FLAG_V;
        }
        return 0;
    }

public:
    ALUStageOutput run(const OperandFetchOutput& in,
                       const ControlSignals& ctrl) const {
        ALUStageOutput out;
        out.branch_target = in.branchTarget;
        out.return_target = in.op1;

        const isa::Word operand_b = ctrl.isImmediate ? in.immx : in.op2;
        out.result = alu.execute(in.op1, operand_b, ctrl, out.flag_values);
        out.flags_written = flag_write_mask(ctrl);
        return out;
    }
};

} // namespace risc::cpu
