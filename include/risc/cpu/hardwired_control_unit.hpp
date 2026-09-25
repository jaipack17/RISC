#pragma once

#include "risc/isa/isa_config.hpp"

namespace risc::cpu {

// Control signals
struct ControlSignals {
    bool isSt = false;
    bool isLd = false;
    bool isBeq = false;
    bool isBgt = false;
    bool isBc = false;
    bool isBv = false;
    bool isRet = false;
    bool isImmediate = false;
    bool isWb = false;
    bool isUBranch = false;
    bool isCall = false;
    bool isHlt = false;

    // aluSignals (at most one is set)
    bool isAdd = false;
    bool isSub = false;
    bool isCmp = false;
    bool isMul = false;
    bool isDiv = false;
    bool isMod = false;
    bool isLsl = false;
    bool isLsr = false;
    bool isAsr = false;
    bool isOr = false;
    bool isAnd = false;
    bool isNot = false;
    bool isXor = false;
    bool isMov = false;
};

// Generates the control signals for an instruction from its opcode and I bit
class HardwiredControlUnit {
public:
    ControlSignals generate(isa::Opcode opcode, bool i_bit) const {
        using isa::Opcode;

        ControlSignals s;
        s.isImmediate = i_bit;
        s.isWb = true; // most instructions write a register, turned off below where not

        switch (opcode) {
            // Arithmetic
            case Opcode::ADD:
                s.isAdd = true;
                break;
            case Opcode::SUB:
                s.isSub = true;
                break;
            case Opcode::MUL:
                s.isMul = true;
                break;
            case Opcode::DIV:
                s.isDiv = true;
                break;
            case Opcode::MOD:
                s.isMod = true;
                break;
            case Opcode::CMP:
                s.isCmp = true;
                s.isWb = false;
                break;

            // Logic & shift
            case Opcode::AND:
                s.isAnd = true;
                break;
            case Opcode::OR:
                s.isOr = true;
                break;
            case Opcode::XOR:
                s.isXor = true;
                break;
            case Opcode::NOT:
                s.isNot = true;
                break;
            case Opcode::LSL:
                s.isLsl = true;
                break;
            case Opcode::LSR:
                s.isLsr = true;
                break;
            case Opcode::ASR:
                s.isAsr = true;
                break;

            // Data movement & system
            case Opcode::MOV:
                s.isMov = true;
                break;
            case Opcode::LD:
                s.isLd = true;
                s.isAdd = true; // address = imm + rs1
                break;
            case Opcode::ST:
                s.isSt = true;
                s.isAdd = true; // address = imm + rs1
                s.isWb = false;
                break;
            case Opcode::NOP:
                s.isWb = false;
                break;
            case Opcode::HLT:
                s.isHlt = true;
                s.isWb = false;
                break;

            // Control flow
            case Opcode::BEQ:
                s.isBeq = true;
                s.isWb = false;
                break;
            case Opcode::BGT:
                s.isBgt = true;
                s.isWb = false;
                break;
            case Opcode::BC:
                s.isBc = true;
                s.isWb = false;
                break;
            case Opcode::BV:
                s.isBv = true;
                s.isWb = false;
                break;
            case Opcode::B:
                s.isUBranch = true;
                s.isWb = false;
                break;
            case Opcode::CALL:
                s.isUBranch = true;
                s.isCall = true;
                break;
            case Opcode::RET:
                s.isUBranch = true;
                s.isRet = true;
                s.isWb = false;
                break;
        }
        return s;
    }
};
}
