#pragma once

#include <array>
#include <cstdint>

#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"

namespace risc::memory {

// 64 KB byte addressable memory
class Memory {
private:
    std::array<std::uint8_t, machine::MEM_SIZE> bytes;

    // Throws if a word at address is outside memory or not aligned
    void check_word_access(isa::Word address) const {
        if (address > machine::MEM_SIZE - isa::WORD_BYTES) {
            throw CpuException{isa::Exception::OutOfBoundsAccess, address};
        }
        if (address % isa::WORD_BYTES != 0) {
            throw CpuException{isa::Exception::MisalignedAccess, address};
        }
    }

    // How far to shift the byte stored at (address + i) within the word
    unsigned byte_shift(unsigned i) const {
        if (machine::ENDIAN == machine::Endian::Little) {
            return 8 * i;
        }
        return 8 * (isa::WORD_BYTES - 1 - i);
    }

public:
    Memory() {
        reset();
    }

    void reset() {
        bytes.fill(0);
    }

    isa::Word read_word(isa::Word address) const {
        check_word_access(address);

        isa::Word value = 0;
        for (unsigned i = 0; i < isa::WORD_BYTES; i++) {
            value = value | (isa::Word(bytes[address + i]) << byte_shift(i));
        }
        return value;
    }

    void write_word(isa::Word address, isa::Word value) {
        check_word_access(address);
        if (address >= machine::TEXT_BASE && address < machine::TEXT_END) {
            throw CpuException{isa::Exception::WriteToText, address};
        }

        for (unsigned i = 0; i < isa::WORD_BYTES; i++) {
            bytes[address + i] = std::uint8_t(value >> byte_shift(i));
        }
    }
};
}
