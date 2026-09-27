#pragma once

#include <cstdint>

#include "risc/cpu/branch_stage_unit.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/cpu/instruction_fetch_unit.hpp"
#include "risc/cpu/memory_access_unit.hpp"
#include "risc/cpu/operand_fetch_unit.hpp"
#include "risc/cpu/pipeline_registers.hpp"
#include "risc/cpu/register_file.hpp"
#include "risc/cpu/register_writeback_unit.hpp"
#include "risc/cpu/cpu.hpp" // CpuStatus, CpuException-facing status contract
#include "risc/isa/machine_config.hpp"
#include "risc/memory/memory.hpp"

namespace risc::cpu {

struct SixStageCycleTrace {
    std::uint64_t cycle = 0;
    SixStagePipelineRegisters stages{};
    bool data_stall = false;
    bool branch_flush = false;
    WritePort write_port{};
};

// Cycle-stepped in-order pipeline: IF -> OF -> ALU -> Branch -> MA -> RW.
// This is separate from CPU, which remains the sequential reference model.
class SixStageCPU {
private:
    memory::Memory& memory;
    RegisterFile rf;
    HardwiredControlUnit cu;
    InstructionFetchUnit ifu;
    OperandFetchUnit ofu;
    ALUStageUnit alu_stage;
    BranchStageUnit branch_stage;
    MemoryAccessUnit mau;
    RegisterWritebackUnit rwu;
    SixStagePipelineRegisters pipeline;
    bool fetch_stopped = false;

    static unsigned field(isa::Word instruction, unsigned pos) {
        return (instruction >> pos) & ((1u << isa::REG_BITS) - 1u);
    }

    static const isa::InstructionInfo* instruction_info(isa::Word instruction) {
        const auto opcode = OperandFetchUnit::opcode(instruction);
        for (const auto& info : isa::INSTRUCTIONS) {
            if (info.opcode == opcode) {
                return &info;
            }
        }
        return nullptr;
    }

    static bool reads_register(isa::Word instruction, const ControlSignals& ctrl,
                               unsigned reg) {
        if (ctrl.isRet) {
            return reg == isa::REG_RA;
        }

        const auto* info = instruction_info(instruction);
        if (info == nullptr) {
            return false; // OF will report the illegal opcode
        }

        switch (info->operands) {
        case isa::Operands::RdRs1Src2:
            return field(instruction, isa::RS1_POS) == reg ||
                   (!ctrl.isImmediate && field(instruction, isa::RS2_POS) == reg);
        case isa::Operands::Rs1Src2:
            return field(instruction, isa::RS1_POS) == reg ||
                   (!ctrl.isImmediate && field(instruction, isa::RS2_POS) == reg);
        case isa::Operands::RdSrc2:
            return !ctrl.isImmediate && field(instruction, isa::RS2_POS) == reg;
        case isa::Operands::RdMem:
            return field(instruction, isa::RS1_POS) == reg ||
                   (ctrl.isSt && field(instruction, isa::RD_POS) == reg);
        case isa::Operands::Label:
        case isa::Operands::None:
            return false;
        }
        return false;
    }

    static bool writes_register(isa::Word instruction, const ControlSignals& ctrl,
                                unsigned reg) {
        if (!ctrl.isWb) {
            return false;
        }
        const unsigned destination = ctrl.isCall
            ? isa::REG_RA
            : field(instruction, isa::RD_POS);
        return destination == reg;
    }

    bool has_data_hazard(isa::Word instruction, const ControlSignals& ctrl) const {
        const auto conflicts_with = [&](isa::Word older_instruction,
                                        const ControlSignals& older_ctrl) {
            for (unsigned reg = 0; reg < isa::NUM_REGS; ++reg) {
                if (writes_register(older_instruction, older_ctrl, reg) &&
                    reads_register(instruction, ctrl, reg)) {
                    return true;
                }
            }
            return false;
        };

        // MA/RW commits at the beginning of this cycle, before OF reads the
        // register file, so it does not need to hold up this instruction.
        return (pipeline.of_alu.valid &&
                conflicts_with(pipeline.of_alu.instruction, pipeline.of_alu.control)) ||
               (pipeline.alu_branch.valid &&
                conflicts_with(pipeline.alu_branch.instruction,
                               pipeline.alu_branch.control)) ||
               (pipeline.branch_ma.valid &&
                conflicts_with(pipeline.branch_ma.instruction,
                               pipeline.branch_ma.control));
    }

    void clear_pipeline() {
        pipeline.reset();
    }

public:
    CpuStatus status = CpuStatus::Running;
    CpuException exception{};
    isa::Word exception_pc = 0;
    std::uint64_t cycles = 0;
    std::uint64_t instructions = 0;
    SixStageCycleTrace last{};

    explicit SixStageCPU(memory::Memory& memory) : memory(memory) {
        reset();
    }

    void reset() {
        rf.reset();
        ifu.reset();
        branch_stage.reset();
        mau.reset();
        pipeline.reset();
        fetch_stopped = false;
        status = CpuStatus::Running;
        exception = CpuException{};
        exception_pc = 0;
        cycles = 0;
        instructions = 0;
        last = SixStageCycleTrace{};
    }

    isa::Word pc() const {
        return ifu.current_pc();
    }

    isa::Word register_value(unsigned index) const {
        return rf.value(index);
    }

    const Flags& flags() const {
        return branch_stage.flags();
    }

    const SixStagePipelineRegisters& pipeline_registers() const {
        return pipeline;
    }

    // Advance every stage once. All stage inputs come from the old pipeline
    // registers; the newly computed register set is committed together.
    void step() {
        if (status != CpuStatus::Running) {
            return;
        }

        ++cycles;
        SixStagePipelineRegisters next{};
        last = SixStageCycleTrace{};
        last.cycle = cycles;

        isa::Word active_pc = ifu.current_pc();
        try {
            // RW: retire the previous cycle's MA result first. OF later in
            // this cycle can therefore observe this write (write-before-read).
            if (pipeline.ma_rw.valid) {
                const auto& in = pipeline.ma_rw;
                rwu.run(in.pc, in.instruction, in.alu_result, in.load_result,
                        in.control, rf);
                last.write_port = rf.write_port;
                ++instructions;
                if (in.control.isHlt) {
                    status = CpuStatus::Halted;
                }
            } else {
                rf.write_port = WritePort{};
            }

            if (status == CpuStatus::Halted) {
                clear_pipeline();
                last.stages = pipeline;
                return;
            }

            // MA: perform the memory operation for the instruction in
            // Branch/MA. Non-memory instructions pass their ALU value through.
            if (pipeline.branch_ma.valid) {
                const auto& in = pipeline.branch_ma;
                active_pc = in.pc;
                mau.load_registers(in.alu.result, in.operands.op2);
                isa::Word loaded = 0;
                if (in.control.isLd) {
                    loaded = memory.read_word(mau.mar);
                    mau.mdr = loaded;
                }
                if (in.control.isSt) {
                    memory.write_word(mau.mar, mau.mdr);
                }
                next.ma_rw = MA_RW_Register{
                    true, in.pc, in.instruction, in.control, in.alu.result, loaded
                };
            }

            // Branch: update condition flags and resolve this instruction.
            BranchStageOutput resolved{};
            if (pipeline.alu_branch.valid) {
                const auto& in = pipeline.alu_branch;
                active_pc = in.pc;
                resolved = branch_stage.run(in.alu, in.control);
                next.branch_ma = Branch_MA_Register{
                    true, in.pc, in.instruction, in.control, in.operands,
                    in.alu, resolved
                };
            }

            const bool redirect = pipeline.alu_branch.valid &&
                                  resolved.is_branch_taken;
            last.branch_flush = redirect;

            // ALU: a taken branch squashes younger instructions before they
            // can cause side effects or exceptions.
            if (!redirect && pipeline.of_alu.valid) {
                const auto& in = pipeline.of_alu;
                active_pc = in.pc;
                const ALUStageOutput alu_out = alu_stage.run(in.operands, in.control);
                next.alu_branch = ALU_Branch_Register{
                    true, in.pc, in.instruction, in.control, in.operands, alu_out
                };
            }

            // OF: decode/control and register read. A dependency inserts a
            // bubble into OF/ALU while holding the waiting instruction in IF/OF.
            bool stalled = false;
            if (!redirect && pipeline.if_of.valid) {
                const auto& in = pipeline.if_of;
                active_pc = in.pc;
                const auto opcode = OperandFetchUnit::opcode(in.instruction);
                const auto control = cu.generate(opcode,
                                                 OperandFetchUnit::i_bit(in.instruction));
                if (has_data_hazard(in.instruction, control)) {
                    stalled = true;
                    next.if_of = in;
                } else {
                    active_pc = in.pc;
                    const auto operands = ofu.run(in.pc, in.instruction, control, rf);
                    next.of_alu = OF_ALU_Register{
                        true, in.pc, in.instruction, control, operands
                    };
                    if (control.isHlt) {
                        fetch_stopped = true;
                    }
                }
            }
            last.data_stall = stalled;

            // IF: hold fetch during a data stall; stop after HLT is decoded.
            // Branch redirects take effect at the next fetch boundary.
            if (redirect) {
                ifu.update_pc(true, resolved.branch_pc);
                fetch_stopped = false;
            } else if (stalled) {
                // Keep the fetch PC and waiting IF/OF entry unchanged.
            } else if (!fetch_stopped && status == CpuStatus::Running) {
                active_pc = ifu.fetch_address();
                const isa::Word instruction = memory.read_word(active_pc);
                next.if_of = IF_OF_Register{true, active_pc, instruction};
                ifu.update_pc(false, 0);
            }

            pipeline = next;
            last.stages = pipeline;
        } catch (const CpuException& e) {
            status = CpuStatus::Exception;
            exception = e;
            exception_pc = active_pc;
            clear_pipeline();
            last.stages = pipeline;
        }
    }

    void run(std::uint64_t max_cycles = 1000000) {
        while (status == CpuStatus::Running && cycles < max_cycles) {
            step();
        }
    }
};

} // namespace risc::cpu
