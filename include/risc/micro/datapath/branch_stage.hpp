#pragma once

#include "risc/micro/datapath/alu_stage.hpp"

namespace risc::cpu::six_stage {

struct BranchStageOutput {
    bool taken = false;
    isa::Word target = 0;
};

class BranchStageUnit {
private:
    Flags flags_state;

    void commit_flags(const ALUStageOutput& alu) {
        constexpr std::uint8_t all_flags =
            isa::FLAG_E | isa::FLAG_GT | isa::FLAG_C | isa::FLAG_V;
        for (std::uint8_t flag = 1; flag <= all_flags; flag <<= 1) {
            if ((alu.flags_written & flag) != 0) {
                flags_state.set(flag, alu.flag_values.is_set(flag));
            }
        }
    }

public:
    void reset() {
        flags_state = Flags{};
    }

    const Flags& flags() const {
        return flags_state;
    }

    BranchStageOutput run(const ALUStageOutput& alu,
                          const ControlSignals& control) {
        commit_flags(alu);

        BranchStageOutput out;
        out.target = control.isRet ? alu.return_target : alu.branch_target;
        const bool beq = control.isBeq && flags_state.is_set(isa::FLAG_E);
        const bool bgt = control.isBgt && flags_state.is_set(isa::FLAG_GT);
        const bool bc = control.isBc && flags_state.is_set(isa::FLAG_C);
        const bool bv = control.isBv && flags_state.is_set(isa::FLAG_V);
        out.taken = control.isUBranch || beq || bgt || bc || bv;
        return out;
    }
};

} // namespace risc::cpu::six_stage
