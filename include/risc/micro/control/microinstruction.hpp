#pragma once

#include <cstdint>

#include "risc/micro/micro_isa.hpp"

namespace risc::micro {

// the 8 microinstructions (3-bit type field)
enum class MicroOp : std::uint8_t { mloadIR = 0, mdecode, mswitch, mmov, mmovi, madd, mbeq, mb };

// type 3 | src 5 | dest 5 | immediate 12 | branchTarget 10 | args 10 = 45 bits
inline constexpr unsigned TYPE_BITS = 3;
inline constexpr unsigned MICRO_REG_BITS = 5;
inline constexpr unsigned MICRO_IMM_BITS = 12;
inline constexpr unsigned MICRO_ADDR_BITS = 10; // µpc width, so 1024 microinstructions
inline constexpr unsigned MICROINSTRUCTION_BITS = 45;
inline constexpr unsigned MICROPROGRAM_SIZE = 1u << MICRO_ADDR_BITS;

// Bit position of each field (its lowest bit)
inline constexpr unsigned ARGS_POS = 0;
inline constexpr unsigned BRANCH_TARGET_POS = ARGS_POS + ARGS_BITS; // 10
inline constexpr unsigned MICRO_IMM_POS = BRANCH_TARGET_POS + MICRO_ADDR_BITS; // 20
inline constexpr unsigned DEST_POS = MICRO_IMM_POS + MICRO_IMM_BITS; // 32
inline constexpr unsigned SRC_POS = DEST_POS + MICRO_REG_BITS; // 37
inline constexpr unsigned TYPE_POS = SRC_POS + MICRO_REG_BITS; // 42

// One row of the microprogram memory
using MicroWord = std::uint64_t;

// A microinstruction, field by field
struct Microinstruction {
    MicroOp type = MicroOp::mb;
    MicroReg src = MicroReg::None;
    MicroReg dest = MicroReg::None;
    int imm = 0; // -2048 .. 2047
    unsigned branchTarget = 0; // µaddress (index into the microprogram memory)
    Args args = NO_ARGS;
};

// Pack the fields into a 45-bit word, ready to store in the microprogram memory
constexpr MicroWord encode(const Microinstruction& m) {
    MicroWord imm_bits = MicroWord(m.imm) & ((MicroWord{1} << MICRO_IMM_BITS) - 1);

    MicroWord word = 0;
    word = word | (MicroWord(m.type) << TYPE_POS);
    word = word | (MicroWord(m.src) << SRC_POS);
    word = word | (MicroWord(m.dest) << DEST_POS);
    word = word | (imm_bits << MICRO_IMM_POS);
    word = word | (MicroWord(m.branchTarget) << BRANCH_TARGET_POS);
    word = word | (MicroWord(m.args) << ARGS_POS);
    return word;
}

// Microassembly
constexpr Microinstruction mloadIR() {
    return {MicroOp::mloadIR};
}

constexpr Microinstruction mdecode() {
    return {MicroOp::mdecode};
}

constexpr Microinstruction mswitch() {
    return {MicroOp::mswitch};
}

constexpr Microinstruction mmov(MicroReg dest, MicroReg src, Args a = NO_ARGS) {
    return {MicroOp::mmov, src, dest, 0, 0, a};
}

constexpr Microinstruction mmovi(MicroReg dest, int imm, Args a = NO_ARGS) {
    return {MicroOp::mmovi, MicroReg::None, dest, imm, 0, a};
}

constexpr Microinstruction madd(MicroReg reg, int imm, Args a = NO_ARGS) {
    return {MicroOp::madd, reg, reg, imm, 0, a};
}

constexpr Microinstruction mbeq(MicroReg reg, int imm, unsigned addr) {
    return {MicroOp::mbeq, reg, MicroReg::None, imm, addr, NO_ARGS};
}

constexpr Microinstruction mb(unsigned addr) {
    return {MicroOp::mb, MicroReg::None, MicroReg::None, 0, addr, NO_ARGS};
}
}
