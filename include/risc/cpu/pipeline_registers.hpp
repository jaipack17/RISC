#pragma once

#include "risc/cpu/alu_stage_unit.hpp"
#include "risc/cpu/branch_stage_unit.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/cpu/operand_fetch_unit.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::cpu {

// Inter-stage state for the six-stage pipeline:
// IF -> OF -> ALU -> Branch -> MA -> RW.
// A false valid bit represents an empty slot (bubble).

struct IF_OF_Register {
    bool valid = false;
    isa::Word pc = 0;
    isa::Word instruction = 0;
};

struct OF_ALU_Register {
    bool valid = false;
    isa::Word pc = 0;
    isa::Word instruction = 0;
    ControlSignals control{};
    OperandFetchOutput operands{};
};

struct ALU_Branch_Register {
    bool valid = false;
    isa::Word pc = 0;
    isa::Word instruction = 0;
    ControlSignals control{};
    OperandFetchOutput operands{}; // retains store data and source context
    ALUStageOutput alu{};
};

struct Branch_MA_Register {
    bool valid = false;
    isa::Word pc = 0;
    isa::Word instruction = 0;
    ControlSignals control{};
    OperandFetchOutput operands{}; // op2 is the value written by ST
    ALUStageOutput alu{};
    BranchStageOutput branch{};
};

struct MA_RW_Register {
    bool valid = false;
    isa::Word pc = 0;
    isa::Word instruction = 0;
    ControlSignals control{};
    isa::Word alu_result = 0;
    isa::Word load_result = 0;
};

// Collect the five boundaries in one resettable value. A future SixStageCPU
// can keep current and next instances and swap them at the end of each cycle.
struct SixStagePipelineRegisters {
    IF_OF_Register if_of{};
    OF_ALU_Register of_alu{};
    ALU_Branch_Register alu_branch{};
    Branch_MA_Register branch_ma{};
    MA_RW_Register ma_rw{};

    void reset() {
        if_of = IF_OF_Register{};
        of_alu = OF_ALU_Register{};
        alu_branch = ALU_Branch_Register{};
        branch_ma = Branch_MA_Register{};
        ma_rw = MA_RW_Register{};
    }
};

} // namespace risc::cpu
