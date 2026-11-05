#pragma once

#include <array>

#include "risc/four-stage-micro/datapath/register_file.hpp"
#include "risc/four-stage-micro/pipeline.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/datapath/alu.hpp"
#include "risc/micro/datapath/datapath.hpp"
#include "risc/micro/datapath/decode_unit.hpp"
#include "risc/micro/datapath/fetch_unit.hpp"
#include "risc/micro/datapath/memory_unit.hpp"
#include "risc/micro/micro_isa.hpp"

namespace risc::four_stage_micro {

using micro::MicroReg;
using micro::TransferMux;

// The microprogrammed datapath's units shared out among the four stages, each stage with its own local bus
class Datapath {
private:
    // The value a register puts on a stage's write bus (0 if it is not on that stage's bus)
    isa::Word value_of(Stage stage, MicroReg reg) {
        const InstructionPacket& p = packet_of(stage);
        switch (reg) {
            case MicroReg::pc:
                if (stage == Stage::IF) {
                    return fetch.pc;
                }
                return p.nextPc;
            case MicroReg::I:
                return p.I;
            case MicroReg::rd:
                return p.rd;
            case MicroReg::rs1:
                return p.rs1;
            case MicroReg::rs2:
                return p.rs2;
            case MicroReg::immx:
                return p.immx;
            case MicroReg::branchTarget:
                return p.branchTarget;
            case MicroReg::regSrc:
                if (stage == Stage::OF) {
                    return register_file.readSrc1;
                }
                if (stage == Stage::EX) {
                    return register_file.readSrc2;
                }
                if (stage == Stage::MW) {
                    return register_file.writeSrc;
                }
                return 0;
            case MicroReg::regData:
                return p.regData;
            case MicroReg::regVal:
                return p.regVal;
            case MicroReg::A:
                return p.A;
            case MicroReg::B:
                return p.B;
            case MicroReg::flagE:
                return alu.flagE;
            case MicroReg::flagGT:
                return alu.flagGT;
            case MicroReg::aluResult:
                return p.aluResult;
            case MicroReg::mar:
                return memory_unit.mar;
            case MicroReg::mdr:
                return memory_unit.mdr;
            case MicroReg::ldResult:
                return p.ldResult;
            case MicroReg::None:
            case MicroReg::ir: // not on the bus
                return 0;
        }
        return 0;
    }

    // Load a register from a stage's read bus (ignored if it is not on that stage's bus)
    void set_value(Stage stage, MicroReg reg, isa::Word value) {
        InstructionPacket& p = packet_of(stage);
        switch (reg) {
            case MicroReg::pc:
                if (stage == Stage::IF) {
                    fetch.pc = value;
                    p.nextPc = value;
                } else if (stage == Stage::EX) {
                    branchTaken = true;
                    branchPc = value;
                }
                break;
            case MicroReg::regSrc:
                if (stage == Stage::OF) {
                    register_file.readSrc1 = value;
                } else if (stage == Stage::EX) {
                    register_file.readSrc2 = value;
                } else if (stage == Stage::MW) {
                    register_file.writeSrc = value;
                }
                break;
            case MicroReg::regData:
                p.regData = value;
                break;
            case MicroReg::regVal:
                p.regVal = value;
                break;
            case MicroReg::A:
                p.A = value;
                break;
            case MicroReg::B:
                p.B = value;
                break;
            case MicroReg::aluResult:
                p.aluResult = value;
                break;
            case MicroReg::mar:
                memory_unit.mar = value;
                break;
            case MicroReg::mdr:
                memory_unit.mdr = value;
                break;
            case MicroReg::ldResult:
                p.ldResult = value;
                break;
            default:
                break; // ir, the decode registers and the flags only drive the write bus
        }
    }

public:
    micro::FetchUnit fetch;
    micro::DecodeUnit decode;
    RegisterFile register_file;
    micro::ALU alu;
    micro::MemoryUnit memory_unit;

    // Pipeline latches. Each stage works on the packet in its input latch.
    InstructionPacket fetched; // IF has no input latch, so it builds its packet here
    InstructionPacket if_of;
    InstructionPacket of_ex;
    InstructionPacket ex_mw;

    // What was on each stage's local bus in its last transfer
    std::array<isa::Word, NUM_STAGES> write_bus{};
    std::array<isa::Word, NUM_STAGES> read_bus{};
    std::array<bool, NUM_STAGES> isMBranch{}; // write bus == µimm, used by mbeq

    std::uint64_t fetch_count = 0; // instructions fetched so far (gives each packet its id)

    // Set when EX loads the pc (a taken branch)
    bool branchTaken = false;
    isa::Word branchPc = 0;

    void reset() {
        fetch.reset();
        decode.reset();
        register_file.reset();
        alu.reset();
        memory_unit.reset();
        fetched = InstructionPacket{};
        if_of = InstructionPacket{};
        of_ex = InstructionPacket{};
        ex_mw = InstructionPacket{};
        write_bus.fill(0);
        read_bus.fill(0);
        isMBranch.fill(false);
        branchTaken = false;
        branchPc = 0;
        fetch_count = 0;
    }

    // The packet a stage works on
    InstructionPacket& packet_of(Stage stage) {
        if (stage == Stage::IF) {
            return fetched;
        }
        if (stage == Stage::OF) {
            return if_of;
        }
        if (stage == Stage::EX) {
            return of_ex;
        }
        return ex_mw;
    }

    // mloadIR on IF's bus: fetch the instruction at pc into IF's packet
    void load_ir(memory::Memory& memory) {
        InstructionPacket& p = fetched;
        fetch.load_ir(memory.read_word(fetch.fetch_address()));
        p.bubble = false;
        fetch_count++;
        p.id = fetch_count;
        p.currentPc = fetch.pc;
        p.nextPc = fetch.pc;
        p.ir = fetch.ir;
    }

    // mdecode on OF's bus: decode OF's instruction into its packet
    void decode_ir() {
        InstructionPacket& p = if_of;
        decode.decode(p.ir, p.currentPc);
        p.I = decode.I;
        p.rd = decode.rd;
        p.rs1 = decode.rs1;
        p.rs2 = decode.rs2;
        p.immx = decode.immx;
        p.branchTarget = decode.branchTarget;
    }

    // One move across a stage's local bus: src -> write bus -> transfer mux -> read bus -> dest
    void transfer(Stage stage, MicroReg src, MicroReg dest, TransferMux mux, isa::Word imm) {
        unsigned s = unsigned(stage);
        write_bus[s] = value_of(stage, src);
        isMBranch[s] = write_bus[s] == imm;

        if (mux == TransferMux::Immediate) {
            read_bus[s] = imm;
        } else if (mux == TransferMux::MicroAdder) {
            read_bus[s] = write_bus[s] + imm;
        } else {
            read_bus[s] = write_bus[s];
        }

        set_value(stage, dest, read_bus[s]);
    }

    // Send args to the unit it names. The unit works on the stage's packet. Returns true for <halt>.
    bool run_args(Stage stage, micro::Args a, memory::Memory& memory) {
        if (a == micro::NO_ARGS) {
            return false;
        }

        InstructionPacket& p = packet_of(stage);
        unsigned opcode = micro::args_opcode(a);
        switch (micro::args_unit(a)) {
            case micro::UnitId::Alu:
                alu.A = p.A;
                alu.B = p.B;
                alu.run(micro::AluOp(opcode));
                p.aluResult = alu.aluResult;
                break;
            case micro::UnitId::RegisterFile:
                if (micro::RegisterFileOp(opcode) == micro::RegisterFileOp::Read && stage == Stage::OF) {
                    register_file.read1();
                    p.regVal = register_file.regVal1;
                } else if (micro::RegisterFileOp(opcode) == micro::RegisterFileOp::Read && stage == Stage::EX) {
                    register_file.read2();
                    p.regVal = register_file.regVal2;
                } else if (micro::RegisterFileOp(opcode) == micro::RegisterFileOp::Write) {
                    register_file.regData = p.regData;
                    register_file.write();
                }
                break;
            case micro::UnitId::Memory:
                memory_unit.run(micro::MemoryOp(opcode), memory);
                p.ldResult = memory_unit.ldResult;
                break;
            case micro::UnitId::Control:
                return micro::ControlOp(opcode) == micro::ControlOp::Halt;
        }
        return false;
    }
};
}
