#pragma once

#include <cstdint>

#include "risc/cpu/execute_unit.hpp"
#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/cpu/instruction_fetch_unit.hpp"
#include "risc/cpu/memory_access_unit.hpp"
#include "risc/cpu/operand_fetch_unit.hpp"
#include "risc/cpu/register_file.hpp"
#include "risc/cpu/register_writeback_unit.hpp"
#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/memory/memory.hpp"

namespace risc::cpu {

enum class CpuStatus { Running, Halted, Exception };

// Information used for visualization
struct CycleTrace {
    std::uint64_t cycle = 0;
    isa::Word pc = 0;
    isa::Word inst = 0;
    ControlSignals ctrl;
    ReadPort read_port1;
    ReadPort read_port2;
    OperandFetchOutput of; // OF: op1, op2, immx, branchTarget
    ExecuteOutput ex; // EX: aluResult, isBranchTaken, branchPC
    isa::Word mar = 0; // MA: address register
    isa::Word mdr = 0; // MA: data register (value stored, or value loaded)
    WritePort write_port; // RW: register file write (E, A, D)
    isa::Word next_pc = 0; // pc after the cycle
};

// Sequential processor
class CPU {
private:
    memory::Memory& memory;
    RegisterFile rf;
    HardwiredControlUnit cu;
    InstructionFetchUnit ifu;
    OperandFetchUnit ofu;
    ExecuteUnit exu;
    MemoryAccessUnit mau;
    RegisterWritebackUnit rwu;

public:
    CpuStatus status = CpuStatus::Running;
    CpuException exception{}; // when status == Exception
    isa::Word exception_pc = 0; // the instruction that caused it
    std::uint64_t cycles = 0;
    std::uint64_t instructions = 0;
    CycleTrace last; // the cycle that just finished

    explicit CPU(memory::Memory& memory) : memory(memory) {
        reset();
    }

    void reset() {
        rf.reset();
        ifu.reset();
        exu.reset();
        mau.reset();
        status = CpuStatus::Running;
        exception = CpuException{};
        exception_pc = 0;
        cycles = 0;
        instructions = 0;
        last = CycleTrace{};
    }

    isa::Word pc() const {
        return ifu.current_pc();
    }

    isa::Word register_value(unsigned index) const {
        return rf.value(index);
    }

    const Flags& flags() const {
        return exu.flags;
    }

    // One clock cycle
    void step() {
        if (status != CpuStatus::Running) {
            return;
        }

        cycles++;
        isa::Word pc = ifu.current_pc();
        last = CycleTrace{};
        last.cycle = cycles;
        last.pc = pc;

        try {
            isa::Word address = ifu.fetch_address();
            isa::Word inst = memory.read_word(address);
            last.inst = inst;

            ControlSignals ctrl = cu.generate(OperandFetchUnit::opcode(inst), OperandFetchUnit::i_bit(inst));
            last.ctrl = ctrl;

            OperandFetchOutput of = ofu.run(pc, inst, ctrl, rf);
            last.read_port1 = rf.read_port1;
            last.read_port2 = rf.read_port2;
            last.of = of;

            ExecuteOutput ex = exu.run(of, ctrl);
            last.ex = ex;

            mau.load_registers(ex.aluResult, of.op2);
            last.mar = mau.mar; // recorded before the access, so a faulting ld/st still shows its address
            last.mdr = mau.mdr;
            if (ctrl.isLd) {
                mau.mdr = memory.read_word(mau.mar);
                last.mdr = mau.mdr;
            }
            if (ctrl.isSt) {
                memory.write_word(mau.mar, mau.mdr);
            }

            rwu.run(pc, inst, ex.aluResult, mau.ldResult(), ctrl, rf);
            last.write_port = rf.write_port;

            instructions++;

            if (ctrl.isHlt) {
                status = CpuStatus::Halted;
            } else {
                ifu.update_pc(ex.isBranchTaken, ex.branchPC);
            }
            last.next_pc = ifu.current_pc();
        } catch (const CpuException& e) {
            status = CpuStatus::Exception;
            exception = e;
            exception_pc = pc;
            last.next_pc = pc;
        }
    }

    // step() until hlt, an exception, or max_cycles
    void run(std::uint64_t max_cycles = 1000000) {
        while (status == CpuStatus::Running && cycles < max_cycles) {
            step();
        }
    }
};
}
