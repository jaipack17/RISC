#pragma once

#include <cstdint>

#include "risc/isa/isa_config.hpp"

namespace risc::machine {

using isa::Word;

// Byte order
enum class Endian : std::uint8_t { Little, Big };
inline constexpr Endian ENDIAN = Endian::Little;

// Memory
inline constexpr Word MEM_SIZE = 0x10000; // 64 KB

// 16KB -> Stack
// 32KB -> Data memory (heap)
// 16KB -> Instruction Memory

// Regions, each is [BASE, END)
inline constexpr Word INSTRUCTION_BASE = 0x0000;
inline constexpr Word INSTRUCTION_END = 0x4000;
inline constexpr Word DATA_BASE = 0x4000;
inline constexpr Word DATA_END = 0xC000;
inline constexpr Word STACK_BASE = 0xC000;
inline constexpr Word STACK_END = 0x10000;

// Reset state
inline constexpr Word RESET_PC = INSTRUCTION_BASE;
inline constexpr Word INITIAL_SP = STACK_END; // empty full-descending stack
}
