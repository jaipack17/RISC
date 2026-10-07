#pragma once

#include <array>
#include <cstdint>

#include "risc/cpu/hardwired_control_unit.hpp"
#include "risc/cpu/operand_fetch_unit.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::cpu::six_stage {

using MicroWord = std::uint32_t;

// Horizontal control-store bit assignments. One microinstruction word carries
// the full set of stage controls for one RISC201 instruction.
enum class MicroControlBit : unsigned {
    Store = 0,
    Load,
    BranchEqual,
    BranchGreater,
    BranchCarry,
    BranchOverflow,
    Return,
    Writeback,
    UnconditionalBranch,
    Call,
    Halt,
    Add,
    Subtract,
    Compare,
    Multiply,
    Divide,
    Modulo,
    ShiftLeft,
    ShiftRightLogical,
    ShiftRightArithmetic,
    Or,
    And,
    Not,
    Xor,
    Move,
};

constexpr MicroWord micro_bit(MicroControlBit bit) {
    return MicroWord{1} << static_cast<unsigned>(bit);
}

struct MicroControlOutput {
    bool valid_opcode = false;
    unsigned micro_pc = 0;
    MicroWord microinstruction = 0;
    ControlSignals signals{};
};

inline constexpr unsigned MICRO_CONTROL_STORE_SIZE = 1u << isa::OPCODE_BITS;

constexpr std::array<MicroWord, MICRO_CONTROL_STORE_SIZE>
make_six_stage_micro_control_store() {
    std::array<MicroWord, MICRO_CONTROL_STORE_SIZE> store{};
    using O = isa::Opcode;
    constexpr auto wb = micro_bit(MicroControlBit::Writeback);
    constexpr auto add = micro_bit(MicroControlBit::Add);
    constexpr auto sub = micro_bit(MicroControlBit::Subtract);

    store[static_cast<unsigned>(O::ADD)] = wb | add;
    store[static_cast<unsigned>(O::SUB)] = wb | sub;
    store[static_cast<unsigned>(O::MUL)] = wb | micro_bit(MicroControlBit::Multiply);
    store[static_cast<unsigned>(O::DIV)] = wb | micro_bit(MicroControlBit::Divide);
    store[static_cast<unsigned>(O::MOD)] = wb | micro_bit(MicroControlBit::Modulo);
    store[static_cast<unsigned>(O::CMP)] = micro_bit(MicroControlBit::Compare);

    store[static_cast<unsigned>(O::AND)] = wb | micro_bit(MicroControlBit::And);
    store[static_cast<unsigned>(O::OR)] = wb | micro_bit(MicroControlBit::Or);
    store[static_cast<unsigned>(O::XOR)] = wb | micro_bit(MicroControlBit::Xor);
    store[static_cast<unsigned>(O::NOT)] = wb | micro_bit(MicroControlBit::Not);
    store[static_cast<unsigned>(O::LSL)] = wb | micro_bit(MicroControlBit::ShiftLeft);
    store[static_cast<unsigned>(O::LSR)] = wb | micro_bit(MicroControlBit::ShiftRightLogical);
    store[static_cast<unsigned>(O::ASR)] = wb | micro_bit(MicroControlBit::ShiftRightArithmetic);

    store[static_cast<unsigned>(O::MOV)] = wb | micro_bit(MicroControlBit::Move);
    store[static_cast<unsigned>(O::LD)] = wb | add | micro_bit(MicroControlBit::Load);
    store[static_cast<unsigned>(O::ST)] = add | micro_bit(MicroControlBit::Store);
    store[static_cast<unsigned>(O::HLT)] = micro_bit(MicroControlBit::Halt);

    store[static_cast<unsigned>(O::BEQ)] = micro_bit(MicroControlBit::BranchEqual);
    store[static_cast<unsigned>(O::BGT)] = micro_bit(MicroControlBit::BranchGreater);
    store[static_cast<unsigned>(O::BC)] = micro_bit(MicroControlBit::BranchCarry);
    store[static_cast<unsigned>(O::BV)] = micro_bit(MicroControlBit::BranchOverflow);
    store[static_cast<unsigned>(O::B)] = micro_bit(MicroControlBit::UnconditionalBranch);
    store[static_cast<unsigned>(O::CALL)] = wb |
        micro_bit(MicroControlBit::UnconditionalBranch) |
        micro_bit(MicroControlBit::Call);
    store[static_cast<unsigned>(O::RET)] =
        micro_bit(MicroControlBit::UnconditionalBranch) |
        micro_bit(MicroControlBit::Return);
    return store;
}

// Per-opcode horizontal control store for the pipelined datapath. The opcode
// dispatch address is the microPC; the fetched control word travels with the
// instruction through the pipeline. ALU and branch decisions remain in their
// respective pipeline stages.
class MicroprogrammedControlUnit {
private:
    static constexpr unsigned store_size = MICRO_CONTROL_STORE_SIZE;
    inline static constexpr auto control_store = make_six_stage_micro_control_store();
    unsigned current_micro_pc = 0;
    MicroWord current_word = 0;
    ControlSignals active_signals{};

    static bool opcode_is_valid(isa::Opcode opcode) {
        for (const auto& instruction : isa::INSTRUCTIONS) {
            if (instruction.opcode == opcode) {
                return true;
            }
        }
        return false;
    }

    static ControlSignals unpack(MicroWord word, bool immediate) {
        const auto has = [word](MicroControlBit bit) {
            return (word & micro_bit(bit)) != 0;
        };
        ControlSignals out;
        out.isSt = has(MicroControlBit::Store);
        out.isLd = has(MicroControlBit::Load);
        out.isBeq = has(MicroControlBit::BranchEqual);
        out.isBgt = has(MicroControlBit::BranchGreater);
        out.isBc = has(MicroControlBit::BranchCarry);
        out.isBv = has(MicroControlBit::BranchOverflow);
        out.isRet = has(MicroControlBit::Return);
        out.isImmediate = immediate;
        out.isWb = has(MicroControlBit::Writeback);
        out.isUBranch = has(MicroControlBit::UnconditionalBranch);
        out.isCall = has(MicroControlBit::Call);
        out.isHlt = has(MicroControlBit::Halt);
        out.isAdd = has(MicroControlBit::Add);
        out.isSub = has(MicroControlBit::Subtract);
        out.isCmp = has(MicroControlBit::Compare);
        out.isMul = has(MicroControlBit::Multiply);
        out.isDiv = has(MicroControlBit::Divide);
        out.isMod = has(MicroControlBit::Modulo);
        out.isLsl = has(MicroControlBit::ShiftLeft);
        out.isLsr = has(MicroControlBit::ShiftRightLogical);
        out.isAsr = has(MicroControlBit::ShiftRightArithmetic);
        out.isOr = has(MicroControlBit::Or);
        out.isAnd = has(MicroControlBit::And);
        out.isNot = has(MicroControlBit::Not);
        out.isXor = has(MicroControlBit::Xor);
        out.isMov = has(MicroControlBit::Move);
        return out;
    }

public:
    void reset() {
        current_micro_pc = 0;
        current_word = 0;
        active_signals = ControlSignals{};
    }

    MicroControlOutput issue(isa::Opcode opcode, bool immediate) {
        current_micro_pc = static_cast<unsigned>(opcode);
        current_word = current_micro_pc < control_store.size()
            ? control_store[current_micro_pc]
            : MicroWord{0};
        active_signals = unpack(current_word, immediate);

        return {
            opcode_is_valid(opcode),
            current_micro_pc,
            current_word,
            active_signals,
        };
    }

    MicroControlOutput decode(isa::Word instruction) {
        return issue(OperandFetchUnit::opcode(instruction),
                     OperandFetchUnit::i_bit(instruction));
    }

    // Same control-generation signature as HardwiredControlUnit, so this
    // controller can replace it in the six-stage OF logic when wired in.
    ControlSignals generate(isa::Opcode opcode, bool immediate) {
        return issue(opcode, immediate).signals;
    }

    unsigned micro_pc() const {
        return current_micro_pc;
    }

    MicroWord current_microinstruction() const {
        return current_word;
    }

    const ControlSignals& current_signals() const {
        return active_signals;
    }

    static constexpr unsigned control_store_size() {
        return store_size;
    }
};

} // namespace risc::cpu::six_stage
