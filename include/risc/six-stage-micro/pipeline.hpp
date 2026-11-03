#pragma once

#include <cstdint>

#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"

namespace risc::six_stage_micro {

enum class Stage : std::uint8_t { IF = 0, OF1, OF2, EX, MA, RW };

inline constexpr unsigned NUM_STAGES = 6;

// Everything an instruction carries from one stage to the next (Sarangi §10.2.3).
// A pipeline latch holds one packet. bubble == true means the latch holds no instruction.
struct InstructionPacket {
    bool bubble = true;
    std::uint64_t id = 0; // for display: 1 for the first instruction fetched, 2 for the next, ...

    // set by IF
    isa::Word currentPc = 0; // address of the instruction
    isa::Word nextPc = 0;    // pc after IF (currentPc + 4)
    isa::Word ir = 0;

    // set by OF1 and OF2
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

    // set by EX and MA
    isa::Word aluResult = 0;
    isa::Word ldResult = 0;

    // set by the stage that raised an exception for this instruction
    bool hasException = false;
    CpuException exception{};
};
}
