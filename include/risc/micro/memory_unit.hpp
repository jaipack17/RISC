#pragma once

#include <cstdint>

#include "risc/isa/isa_config.hpp"
#include "risc/memory/memory.hpp"

namespace risc::micro {

// Memory unit args
enum class MemoryOp : std::uint8_t { Nop = 0, Load = 1, Store = 2 };

class MemoryUnit {
public:
    isa::Word mar = 0; // memory address register
    isa::Word mdr = 0; // memory data register
    isa::Word ldResult = 0;

    void reset() {
        mar = 0;
        mdr = 0;
        ldResult = 0;
    }

    void run(MemoryOp op, memory::Memory& memory) {
        if (op == MemoryOp::Load) {
            ldResult = memory.read_word(mar);
        } else if (op == MemoryOp::Store) {
            memory.write_word(mar, mdr);
        }
    }
};
}
