#pragma once

#include "risc/cpu/alu_stage_unit.hpp"
#include "risc/cpu/flags.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::cpu {

struct BranchStageOutput {
    bool is_branch_taken = false;
    isa::Word branch_pc = 0;
};

// Commits ALU-produced condition flags and resolves control-flow instructions.
class BranchStageUnit {
private:
    Flags condition_flags;

    void commit_flags(const ALUStageOutput& in) {
        constexpr std::uint8_t all_flags =
            isa::FLAG_E | isa::FLAG_GT | isa::FLAG_C | isa::FLAG_V;
        for (std::uint8_t flag = 1; flag <= all_flags; flag <<= 1) {
            if ((in.flags_written & flag) != 0) {
                condition_flags.set(flag, in.flag_values.is_set(flag));
            }
        }
    }

public:
    void reset() {
        condition_flags = Flags{};
    }

    const Flags& flags() const {
        return condition_flags;
    }

    BranchStageOutput run(const ALUStageOutput& in,
                          const ControlSignals& ctrl) {
        // Commit flags before resolving this instruction. Current ISA
        // instructions do not both write flags and branch.
        commit_flags(in);

        BranchStageOutput out;
        out.branch_pc = ctrl.isRet ? in.return_target : in.branch_target;

        const bool beq_taken = ctrl.isBeq && condition_flags.is_set(isa::FLAG_E);
        const bool bgt_taken = ctrl.isBgt && condition_flags.is_set(isa::FLAG_GT);
        const bool bc_taken = ctrl.isBc && condition_flags.is_set(isa::FLAG_C);
        const bool bv_taken = ctrl.isBv && condition_flags.is_set(isa::FLAG_V);
        out.is_branch_taken = ctrl.isUBranch || beq_taken || bgt_taken ||
                              bc_taken || bv_taken;
        return out;
    }
};

} // namespace risc::cpu
