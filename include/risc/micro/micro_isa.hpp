#pragma once

#include <cstdint>

#include "risc/micro/alu.hpp"
#include "risc/micro/memory_unit.hpp"
#include "risc/micro/register_file.hpp"

namespace risc::micro {

// Internal registers visible to microcode (Table 9.7), used as the 5-bit src/dest ids
enum class MicroReg : std::uint8_t {
    None = 0, // no register (e.g. mb, mloadIR)
    pc,
    ir,
    I,
    rd,
    rs1,
    rs2,
    immx,
    branchTarget,
    regSrc,
    regData,
    regVal,
    A,
    B,
    flagE,
    flagGT,
    aluResult,
    mar,
    mdr,
    ldResult,
};

// args = unit id (3 bits) + opcode (7 bits), args == 0 means no operation (§9.7.2)
inline constexpr unsigned ARGS_UNIT_BITS = 3;
inline constexpr unsigned ARGS_OPCODE_BITS = 7;
inline constexpr unsigned ARGS_BITS = ARGS_UNIT_BITS + ARGS_OPCODE_BITS;

enum class UnitId : std::uint8_t { Alu = 0, RegisterFile = 1, Memory = 2, Control = 3 };

// Commands to the µcontrol unit itself, needed for hlt
enum class ControlOp : std::uint8_t { Nop = 0, Halt = 1 };

using Args = std::uint16_t;

inline constexpr Args NO_ARGS = 0;

constexpr Args make_args(UnitId unit, unsigned opcode) {
    return Args((unsigned(unit) << ARGS_OPCODE_BITS) | opcode);
}

constexpr UnitId args_unit(Args args) {
    return UnitId(args >> ARGS_OPCODE_BITS);
}

constexpr unsigned args_opcode(Args args) {
    return args & ((1u << ARGS_OPCODE_BITS) - 1);
}

// So microcode can write <add>, <read>, <load> directly, e.g. args(AluOp::Add)
constexpr Args args(AluOp op) {
    return make_args(UnitId::Alu, unsigned(op));
}

constexpr Args args(RegisterFileOp op) {
    return make_args(UnitId::RegisterFile, unsigned(op));
}

constexpr Args args(MemoryOp op) {
    return make_args(UnitId::Memory, unsigned(op));
}

constexpr Args args(ControlOp op) {
    return make_args(UnitId::Control, unsigned(op));
}
}
