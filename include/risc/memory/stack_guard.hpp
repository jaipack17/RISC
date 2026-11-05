#pragma once

#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"

namespace risc::memory {

// Stack boundary check, done whenever a register file is about to write sp.
// The stack is full-descending: sp == STACK_END is an empty stack, sp == STACK_BASE a full one.
// A push past the bottom raises StackOverflow, a pop past the top StackUnderflow.
// The register is not written when the check fails (the exception is precise).
inline void check_stack_pointer(isa::Word sp) {
    if (sp < machine::STACK_BASE) {
        throw CpuException{isa::Exception::StackOverflow, sp};
    }
    if (sp > machine::STACK_END) {
        throw CpuException{isa::Exception::StackUnderflow, sp};
    }
}
}
