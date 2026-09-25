#pragma once

#include "risc/isa/isa_config.hpp"

namespace risc::cpu {

class MemoryAccessUnit {
public:
    isa::Word mar = 0; // memory address register
    isa::Word mdr = 0; // memory data register

    void reset() {
        mar = 0;
        mdr = 0;
    }

    // mar = aluResult (the address), mdr = op2 (the value to store)
    void load_registers(isa::Word aluResult, isa::Word op2) {
        mar = aluResult;
        mdr = op2;
    }

    isa::Word ldResult() const {
        return mdr;
    }
};
}
