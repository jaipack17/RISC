#pragma once

#include "risc/micro/datapath/branch_stage.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/cpu/operand_fetch_unit.hpp"

namespace risc::cpu::six_stage {

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
    OperandFetchOutput operands{};
    ALUStageOutput alu{};
};

struct Branch_MA_Register {
    bool valid = false;
    isa::Word pc = 0;
    isa::Word instruction = 0;
    ControlSignals control{};
    OperandFetchOutput operands{};
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

struct PipelineRegisters {
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

} // namespace risc::cpu::six_stage
