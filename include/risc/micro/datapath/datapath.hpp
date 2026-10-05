#pragma once

#include <cstdint>

#include "risc/isa/isa_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/datapath/alu.hpp"
#include "risc/micro/datapath/decode_unit.hpp"
#include "risc/micro/datapath/fetch_unit.hpp"
#include "risc/micro/datapath/memory_unit.hpp"
#include "risc/micro/datapath/register_file.hpp"
#include "risc/micro/micro_isa.hpp"

namespace risc::micro {

// Transfer multiplexer: what goes onto the read bus (Figure 9.23)
enum class TransferMux : std::uint8_t { WriteBus, Immediate, MicroAdder };

// Figure 9.22: all the units, joined by the shared bus of Figure 9.23
class Datapath {
private:
    // The value a register puts on the write bus (0 if it is not connected to it)
    isa::Word value_of(MicroReg reg) const {
        switch (reg) {
            case MicroReg::pc:
                return fetch.pc;
            case MicroReg::I:
                return decode.I;
            case MicroReg::rd:
                return decode.rd;
            case MicroReg::rs1:
                return decode.rs1;
            case MicroReg::rs2:
                return decode.rs2;
            case MicroReg::immx:
                return decode.immx;
            case MicroReg::branchTarget:
                return decode.branchTarget;
            case MicroReg::regSrc:
                return register_file.regSrc;
            case MicroReg::regData:
                return register_file.regData;
            case MicroReg::regVal:
                return register_file.regVal;
            case MicroReg::A:
                return alu.A;
            case MicroReg::B:
                return alu.B;
            case MicroReg::flagE:
                return alu.flagE;
            case MicroReg::flagGT:
                return alu.flagGT;
            case MicroReg::aluResult:
                return alu.aluResult;
            case MicroReg::mar:
                return memory_unit.mar;
            case MicroReg::mdr:
                return memory_unit.mdr;
            case MicroReg::ldResult:
                return memory_unit.ldResult;
            case MicroReg::None:
            case MicroReg::ir: // not on the bus
                return 0;
        }
        return 0;
    }

    // Load a register from the read bus (ignored if it is not connected to it)
    void set_value(MicroReg reg, isa::Word value) {
        switch (reg) {
            case MicroReg::pc:
                fetch.pc = value;
                break;
            case MicroReg::regSrc:
                register_file.regSrc = value;
                break;
            case MicroReg::regData:
                register_file.regData = value;
                break;
            case MicroReg::regVal:
                register_file.regVal = value;
                break;
            case MicroReg::A:
                alu.A = value;
                break;
            case MicroReg::B:
                alu.B = value;
                break;
            case MicroReg::aluResult:
                alu.aluResult = value;
                break;
            case MicroReg::mar:
                memory_unit.mar = value;
                break;
            case MicroReg::mdr:
                memory_unit.mdr = value;
                break;
            case MicroReg::ldResult:
                memory_unit.ldResult = value;
                break;
            default:
                break; // ir, the decode registers and the flags only drive the write bus
        }
    }

public:
    FetchUnit fetch;
    DecodeUnit decode;
    RegisterFile register_file;
    ALU alu;
    MemoryUnit memory_unit;

    // What was on the shared bus in the last transfer
    isa::Word write_bus = 0;
    isa::Word read_bus = 0;
    bool isMBranch = false; // write bus == µimm, used by mbeq

    void reset() {
        fetch.reset();
        decode.reset();
        register_file.reset();
        alu.reset();
        memory_unit.reset();
        write_bus = 0;
        read_bus = 0;
        isMBranch = false;
    }

    // One move across the shared bus: src -> write bus -> transfer mux -> read bus -> dest
    void transfer(MicroReg src, MicroReg dest, TransferMux mux, isa::Word imm) {
        write_bus = value_of(src);
        isMBranch = write_bus == imm;

        if (mux == TransferMux::Immediate) {
            read_bus = imm;
        } else if (mux == TransferMux::MicroAdder) {
            read_bus = write_bus + imm;
        } else {
            read_bus = write_bus;
        }

        set_value(dest, read_bus);
    }

    // Send args to the unit it names. Returns true for <halt>.
    bool run_args(Args a, memory::Memory& memory) {
        unsigned opcode = args_opcode(a);
        switch (args_unit(a)) {
            case UnitId::Alu:
                alu.run(AluOp(opcode));
                break;
            case UnitId::RegisterFile:
                register_file.run(RegisterFileOp(opcode));
                break;
            case UnitId::Memory:
                memory_unit.run(MemoryOp(opcode), memory);
                break;
            case UnitId::Control:
                return ControlOp(opcode) == ControlOp::Halt;
        }
        return false;
    }
};
}
