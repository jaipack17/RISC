#pragma once

#include "risc/isa/isa_config.hpp"

namespace risc {
struct CpuException {
    isa::Exception kind;
    isa::Word address = 0;
};
}
