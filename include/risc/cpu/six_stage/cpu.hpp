#pragma once

#include <cstdint>

#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/control/micro_control_unit.hpp"
#include "risc/micro/control/micro_decode_unit.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/micro/datapath/datapath.hpp"
#include "risc/microassembler/microassembler.hpp"

namespace risc::cpu::six_stage {

enum class Status { Running, Halted, Exception };

// A microinstruction is classified by the six-stage function it serves.
// Multiple microinstructions may belong to the same phase, and some
// instruction routines may not need every phase.
enum class Stage { IF, OF, ALU, Branch, MA, RW };

struct CycleTrace {
    std::uint64_t cycle = 0;
    isa::Word instruction_pc = 0;
    isa::Word pc = 0;
    unsigned micro_pc = 0;
    unsigned next_micro_pc = 0;
    micro::MicroWord microinstruction = 0;
    micro::MicroOp micro_op = micro::MicroOp::mb;
    Stage stage = Stage::IF;
};

inline const char* stage_name(Stage stage) {
    switch (stage) {
    case Stage::IF: return "IF";
    case Stage::OF: return "OF";
    case Stage::ALU: return "ALU";
    case Stage::Branch: return "Branch";
    case Stage::MA: return "MA";
    case Stage::RW: return "RW";
    }
    return "?";
}

// Sequential CPU controlled by the project's assembled vertical microcode.
// Each step executes one microinstruction; stages are functional phases,
// so an ISA instruction can take a different number of cycles by routine.
class CPU {
private:
    memory::Memory& memory;
    micro::Datapath datapath;
    micro::MicroControlUnit control_unit;
    Stage previous_stage = Stage::IF;
    bool instruction_active = false;
    isa::Word active_instruction_pc = 0;

    static bool has_args(micro::Args args) {
        return args != micro::NO_ARGS;
    }

    static Stage classify(const micro::Microinstruction& instruction,
                          Stage previous) {
        using micro::MicroOp;
        using micro::MicroReg;
        using micro::UnitId;

        if (instruction.type == MicroOp::mloadIR) return Stage::IF;
        if (instruction.type == MicroOp::mdecode ||
            instruction.type == MicroOp::mswitch) return Stage::OF;

        if (instruction.type == MicroOp::mbeq) {
            return instruction.src == MicroReg::flagE ||
                           instruction.src == MicroReg::flagGT ||
                           instruction.src == MicroReg::flagC ||
                           instruction.src == MicroReg::flagV
                ? Stage::Branch
                : Stage::OF; // e.g. selecting register vs immediate operands
        }

        if (instruction.type == MicroOp::mb) {
            // mb changes only the microPC; attribute it to the operation
            // whose control flow it completes.
            return previous;
        }

        if (has_args(instruction.args)) {
            switch (micro::args_unit(instruction.args)) {
            case UnitId::Alu: return Stage::ALU;
            case UnitId::RegisterFile:
                return micro::args_opcode(instruction.args) ==
                               unsigned(micro::RegisterFileOp::Write)
                    ? Stage::RW : Stage::OF;
            case UnitId::Memory: return Stage::MA;
            case UnitId::Control: return Stage::RW;
            }
        }

        if (instruction.dest == MicroReg::pc) {
            return instruction.src == MicroReg::branchTarget ||
                           instruction.src == MicroReg::regVal
                ? Stage::Branch : Stage::IF;
        }
        if (instruction.dest == MicroReg::mar ||
            instruction.dest == MicroReg::mdr ||
            instruction.dest == MicroReg::ldResult) return Stage::MA;
        if (instruction.dest == MicroReg::regData ||
            (instruction.dest == MicroReg::regSrc &&
             instruction.src == MicroReg::rd)) return Stage::RW;
        if (instruction.dest == MicroReg::A || instruction.dest == MicroReg::B ||
            instruction.src == MicroReg::aluResult) return Stage::ALU;
        return Stage::OF;
    }

public:
    Status status = Status::Running;
    CpuException exception{};
    isa::Word exception_pc = 0;
    std::uint64_t cycles = 0;
    std::uint64_t instructions_retired = 0;
    CycleTrace last{};

    CPU(memory::Memory& memory, const microassembler::Microprogram& program)
        : memory(memory) {
        load_microprogram(program);
        reset();
    }

    void load_microprogram(const microassembler::Microprogram& program) {
        control_unit.write_microprogram_words(program.words);
        for (const microassembler::Entry& entry : program.entries) {
            control_unit.switch_unit.set_start(entry.opcode, entry.address);
        }
    }

    void reset() {
        datapath.reset();
        control_unit.reset();
        previous_stage = Stage::IF;
        instruction_active = false;
        active_instruction_pc = 0;
        status = Status::Running;
        exception = CpuException{};
        exception_pc = 0;
        cycles = 0;
        instructions_retired = 0;
        last = CycleTrace{};
    }

    isa::Word pc() const { return datapath.fetch.pc; }

    isa::Word register_value(unsigned index) const {
        return datapath.register_file.value(index);
    }

    unsigned micro_pc() const { return control_unit.upc; }

    const micro::Datapath& micro_datapath() const { return datapath; }

    void step() {
        if (status != Status::Running || control_unit.halted) return;

        const unsigned address = control_unit.upc;
        if (address >= control_unit.microprogram.size()) {
            status = Status::Exception;
            exception = CpuException{isa::Exception::IllegalInstruction,
                                     datapath.fetch.pc};
            exception_pc = datapath.fetch.pc;
            return;
        }

        if (address == 0) {
            if (instruction_active) {
                ++instructions_retired;
                instruction_active = false;
            }
            instruction_active = true;
            active_instruction_pc = datapath.fetch.pc;
        }

        const micro::MicroWord word = control_unit.microprogram[address];
        const micro::Microinstruction instruction =
            micro::MicroDecodeUnit::fields(word);
        const Stage active_stage = classify(instruction, previous_stage);

        ++cycles;
        last = CycleTrace{cycles, active_instruction_pc, datapath.fetch.pc,
                          address, address, word, instruction.type, active_stage};

        try {
            control_unit.step(datapath, memory);
            last.next_micro_pc = control_unit.upc;
            last.pc = datapath.fetch.pc;
            if (control_unit.halted) {
                if (instruction_active) ++instructions_retired;
                instruction_active = false;
                status = Status::Halted;
            }
            previous_stage = active_stage;
        } catch (const CpuException& error) {
            status = Status::Exception;
            exception = CpuException{error.kind, active_instruction_pc};
            exception_pc = active_instruction_pc;
        }
    }

    void run(std::uint64_t max_cycles = 1000000) {
        while (status == Status::Running && cycles < max_cycles) step();
    }
};

} // namespace risc::cpu::six_stage
