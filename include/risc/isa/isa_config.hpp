#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace risc::isa {

// Word
inline constexpr unsigned WORD_BITS  = 32;
inline constexpr unsigned WORD_BYTES = WORD_BITS / 8;
using Word  = std::uint32_t;
using SWord = std::int32_t;

// Register file
inline constexpr unsigned REG_BITS = 4;
inline constexpr unsigned NUM_REGS = 1u << REG_BITS;
inline constexpr unsigned REG_SP   = 14;
inline constexpr unsigned REG_RA   = 15;

// Flags (one bit each in the flags register)
inline constexpr std::uint8_t FLAG_E  = 1 << 0;
inline constexpr std::uint8_t FLAG_GT = 1 << 1;
inline constexpr std::uint8_t FLAG_C  = 1 << 2;
inline constexpr std::uint8_t FLAG_V  = 1 << 3;

// Instruction formats
enum class Format : std::uint8_t { Branch, Register, Immediate };

inline constexpr unsigned INSTR_BITS  = 32;
inline constexpr unsigned OPCODE_BITS = 5;
inline constexpr unsigned MOD_BITS    = 2;

// Bit position of each field (its lowest bit)
inline constexpr unsigned OPCODE_POS = INSTR_BITS - OPCODE_BITS;   // 27
inline constexpr unsigned I_POS      = OPCODE_POS - 1;             // 26
inline constexpr unsigned RD_POS     = I_POS - REG_BITS;           // 22
inline constexpr unsigned RS1_POS    = RD_POS - REG_BITS;          // 18
inline constexpr unsigned RS2_POS    = RS1_POS - REG_BITS;         // 14
inline constexpr unsigned MOD_POS    = RS1_POS - MOD_BITS;         // 16
inline constexpr unsigned IMM_POS    = 0;
inline constexpr unsigned OFFSET_POS = 0;

// Widths of the fields that take up the remaining bits
inline constexpr unsigned IMM_BITS    = MOD_POS;                   // 16
inline constexpr unsigned OFFSET_BITS = OPCODE_POS;                // 27

// Immediate modifiers
enum class Modifier : std::uint8_t { Default = 0b00, Unsigned = 0b01, High = 0b10 };

// Opcodes
enum class Opcode : std::uint8_t {
    // 00 arithmetic
    ADD = 0b00000, SUB = 0b00001, MUL = 0b00010, DIV = 0b00011, MOD = 0b00100, CMP = 0b00101,
    // 01 logic & shift
    AND = 0b01000, OR  = 0b01001, XOR = 0b01010, NOT = 0b01011,
    LSL = 0b01100, LSR = 0b01101, ASR = 0b01110,
    // 10 data movement & system
    MOV = 0b10000, LD  = 0b10001, ST  = 0b10010, NOP = 0b10011, HLT = 0b10100,
    // 11 control flow
    BEQ = 0b11000, BGT = 0b11001, BC  = 0b11010, BV  = 0b11011,
    B   = 0b11100, CALL = 0b11101, RET = 0b11110,
};

// Operand shapes
enum class Operands : std::uint8_t {
    RdRs1Src2,   // add rd, rs1, rs2/imm
    Rs1Src2,     // cmp rs1, rs2/imm
    RdSrc2,      // mov rd, rs2/imm
    RdMem,       // ld  rd, imm[rs1]
    Label,       // beq label
    None,        // ret
};

// Instruction table
struct InstructionInfo {
    std::string_view mnemonic;
    Opcode opcode;
    Operands operands;
    std::uint8_t flags_written;
    std::uint8_t flags_read;
};

inline constexpr unsigned NUM_INSTRUCTIONS = 25;

inline constexpr std::array<InstructionInfo, NUM_INSTRUCTIONS> INSTRUCTIONS = {{
    {"add", Opcode::ADD, Operands::RdRs1Src2, FLAG_C | FLAG_V, 0},
    {"sub", Opcode::SUB, Operands::RdRs1Src2, FLAG_C | FLAG_V, 0},
    {"mul", Opcode::MUL, Operands::RdRs1Src2, 0, 0},
    {"div", Opcode::DIV, Operands::RdRs1Src2, 0, 0},
    {"mod", Opcode::MOD, Operands::RdRs1Src2, 0, 0},
    {"cmp", Opcode::CMP, Operands::Rs1Src2, FLAG_E | FLAG_GT | FLAG_C | FLAG_V, 0},
    {"and", Opcode::AND, Operands::RdRs1Src2, 0, 0},
    {"or", Opcode::OR, Operands::RdRs1Src2, 0, 0},
    {"xor", Opcode::XOR, Operands::RdRs1Src2, 0, 0},
    {"not", Opcode::NOT, Operands::RdSrc2, 0, 0},
    {"lsl", Opcode::LSL, Operands::RdRs1Src2, 0, 0},
    {"lsr", Opcode::LSR, Operands::RdRs1Src2, 0, 0},
    {"asr", Opcode::ASR, Operands::RdRs1Src2, 0, 0},
    {"mov", Opcode::MOV, Operands::RdSrc2, 0, 0},
    {"ld", Opcode::LD, Operands::RdMem, 0, 0},
    {"st", Opcode::ST, Operands::RdMem, 0, 0},
    {"nop", Opcode::NOP, Operands::None, 0, 0},
    {"hlt", Opcode::HLT, Operands::None, 0, 0},
    {"beq", Opcode::BEQ, Operands::Label, 0, FLAG_E},
    {"bgt", Opcode::BGT, Operands::Label, 0, FLAG_GT},
    {"bc", Opcode::BC, Operands::Label, 0, FLAG_C},
    {"bv", Opcode::BV, Operands::Label, 0, FLAG_V},
    {"b", Opcode::B, Operands::Label, 0, 0},
    {"call", Opcode::CALL, Operands::Label, 0, 0},
    {"ret", Opcode::RET, Operands::None, 0, 0},
}};

// Exceptions
enum class Exception : std::uint8_t {
    IllegalInstruction, // unused opcode, or modifier 0b11
    MisalignedAccess, // ld/st address not a multiple of 4
    MisalignedFetch, // PC not a multiple of 4
    OutOfBoundsAccess, // address >= MEM_SIZE
    WriteToText, // st into the TEXT region
    ExecuteOutsideText, // PC left the TEXT region
    DivideByZero, // div/mod with divisor 0
    StackOverflow, // rules decided in the stack phase
    StackUnderflow, // rules decided in the stack phase
};
}
