#pragma once

#include <cstdint>

#include "risc/alu/alu.hpp"
#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::micro {

// ALU commands, carried on the args wires (0 = nop)
enum class AluOp : std::uint8_t {
    Nop = 0,
    Add,
    Sub,
    Cmp, // sets E, GT
    Mul,
    Div,
    Mod,
    Lsl,
    Lsr,
    Asr,
    And,
    Or,
    Xor,
    Not, // ~B
};

class ALU {
private:
    using Word = isa::Word;
    using SWord = isa::SWord;

    alu::OptimizedALU core_alu;

public:
    Word A = 0;
    Word B = 0;
    Word aluResult = 0;
    bool flagE = false;
    bool flagGT = false;

    explicit ALU(alu::AdderAlgorithm adder = alu::AdderAlgorithm::CarryLookahead,
                 alu::MultiplierAlgorithm multiplier = alu::MultiplierAlgorithm::BoothRadix2,
                 alu::DividerAlgorithm divider = alu::DividerAlgorithm::Restoring)
        : core_alu(adder, multiplier, divider) {}

    void reset() {
        A = 0;
        B = 0;
        aluResult = 0;
        flagE = false;
        flagGT = false;
    }

    void select_adder(alu::AdderAlgorithm algo) { core_alu.select_adder(algo); }
    void select_multiplier(alu::MultiplierAlgorithm algo) { core_alu.select_multiplier(algo); }
    void select_divider(alu::DividerAlgorithm algo) { core_alu.select_divider(algo); }

    const alu::OptimizedALU& core() const { return core_alu; }
    alu::OptimizedALU& core() { return core_alu; }

    // Carry out the command from args on A and B
    void run(AluOp op) {
        alu::ALUFlags flags;

        switch (op) {
            case AluOp::Nop:
                break;
            case AluOp::Add:
                aluResult = core_alu.add(A, B, flags);
                break;
            case AluOp::Sub:
                aluResult = core_alu.sub(A, B, flags);
                break;
            case AluOp::Cmp:
                core_alu.cmp(A, B, flags);
                flagE = flags.E;
                flagGT = flags.GT;
                break;
            case AluOp::Mul:
                aluResult = core_alu.mul(A, B);
                break;
            case AluOp::Div:
                aluResult = core_alu.div(A, B);
                break;
            case AluOp::Mod:
                aluResult = core_alu.mod(A, B);
                break;
            case AluOp::Lsl:
                aluResult = core_alu.shift(A, B, alu::ShiftOp::Lsl);
                break;
            case AluOp::Lsr:
                aluResult = core_alu.shift(A, B, alu::ShiftOp::Lsr);
                break;
            case AluOp::Asr:
                aluResult = core_alu.shift(A, B, alu::ShiftOp::Asr);
                break;
            case AluOp::And:
                aluResult = alu::OptimizedALU::bitwise_and(A, B);
                break;
            case AluOp::Or:
                aluResult = alu::OptimizedALU::bitwise_or(A, B);
                break;
            case AluOp::Xor:
                aluResult = alu::OptimizedALU::bitwise_xor(A, B);
                break;
            case AluOp::Not:
                aluResult = alu::OptimizedALU::bitwise_not(B);
                break;
        }
    }
};

} // namespace risc::micro
