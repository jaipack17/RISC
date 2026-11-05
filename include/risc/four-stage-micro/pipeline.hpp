#pragma once

#include <cstdint>

#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::four_stage_micro {

// IF | OF | EX | MW: the six-stage pipeline with OF2 merged into EX and MA merged into RW
enum class Stage : std::uint8_t { IF = 0, OF, EX, MW };

inline constexpr unsigned NUM_STAGES = 4;

// Why a latch holds no instruction: empty since the start, a bubble the interlock put in behind a
// waiting instruction, a slot a taken branch (or an exception) flushed, or nothing fetched after a hlt.
// For display and the Compare page's cycle accounting only; the processor never reads it.
enum class BubbleReason : std::uint8_t { Fill, Stall, Flush, Drain };

// Everything an instruction carries from one stage to the next.
// A pipeline latch holds one packet. bubble == true means the latch holds no instruction.
struct InstructionPacket {
    bool bubble = true;
    BubbleReason reason = BubbleReason::Fill; // when bubble: why the latch is empty
    std::uint64_t id = 0; // for display: 1 for the first instruction fetched, 2 for the next, ...

    // set by IF
    isa::Word currentPc = 0; // address of the instruction
    isa::Word nextPc = 0;    // pc after IF (currentPc + 4)
    isa::Word ir = 0;

    // set by OF and EX
    bool I = false;
    unsigned rd = 0;
    unsigned rs1 = 0;
    unsigned rs2 = 0;
    isa::Word immx = 0;
    isa::Word branchTarget = 0;
    isa::Word A = 0;
    isa::Word B = 0;
    isa::Word regVal = 0;
    isa::Word regData = 0;

    // set by EX and MW
    isa::Word aluResult = 0;
    isa::Word ldResult = 0;

    // set by the stage that raised an exception for this instruction
    bool hasException = false;
    CpuException exception{};
};

// An empty latch, and why it is empty
inline InstructionPacket bubble(BubbleReason reason) {
    InstructionPacket p;
    p.reason = reason;
    return p;
}
}
