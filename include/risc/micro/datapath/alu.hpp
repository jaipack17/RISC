#pragma once

#include <cstdint>
#include <limits>

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

    // Signed, rounds toward zero, remainder takes the sign of A
    Word divide(bool remainder) {
        if (B == 0) {
            throw CpuException{isa::Exception::DivideByZero, 0};
        }
        SWord x = SWord(A);
        SWord y = SWord(B);
        if (x == std::numeric_limits<SWord>::min() && y == -1) {
            if (remainder) {
                return 0;
            }
            return A;
        }
        if (remainder) {
            return Word(x % y);
        }
        return Word(x / y);
    }

public:
    Word A = 0;
    Word B = 0;
    Word aluResult = 0;
    bool flagE = false;
    bool flagGT = false;
    bool flagC = false;
    bool flagV = false;

    void reset() {
        A = 0;
        B = 0;
        aluResult = 0;
        flagE = false;
        flagGT = false;
        flagC = false;
        flagV = false;
    }

    void set_add_flags(bool subtract) {
        const Word result = subtract ? A - B : A + B;
        if (subtract) {
            flagC = A >= B;
            const std::int64_t exact = std::int64_t(SWord(A)) - std::int64_t(SWord(B));
            flagV = exact != std::int64_t(SWord(result));
        } else {
            flagC = (std::uint64_t(A) + std::uint64_t(B)) >> isa::WORD_BITS != 0;
            const std::int64_t exact = std::int64_t(SWord(A)) + std::int64_t(SWord(B));
            flagV = exact != std::int64_t(SWord(result));
        }
    }

    // Carry out the command from args on A and B
    void run(AluOp op) {
        unsigned shift = B & (isa::WORD_BITS - 1); // only the low 5 bits of B

        switch (op) {
            case AluOp::Nop:
                break;
            case AluOp::Add:
                aluResult = A + B;
                set_add_flags(false);
                break;
            case AluOp::Sub:
                aluResult = A - B;
                set_add_flags(true);
                break;
            case AluOp::Cmp:
                flagE = A == B;
                flagGT = SWord(A) > SWord(B);
                set_add_flags(true);
                break;
            case AluOp::Mul:
                aluResult = A * B;
                break;
            case AluOp::Div:
                aluResult = divide(false);
                break;
            case AluOp::Mod:
                aluResult = divide(true);
                break;
            case AluOp::Lsl:
                aluResult = A << shift;
                break;
            case AluOp::Lsr:
                aluResult = A >> shift;
                break;
            case AluOp::Asr:
                aluResult = Word(SWord(A) >> shift);
                break;
            case AluOp::And:
                aluResult = A & B;
                break;
            case AluOp::Or:
                aluResult = A | B;
                break;
            case AluOp::Xor:
                aluResult = A ^ B;
                break;
            case AluOp::Not:
                aluResult = ~B;
                break;
        }
    }
};
}
