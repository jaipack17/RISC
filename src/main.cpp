#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "risc/cpu/cpu.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/visualizer/visualizer.hpp"

using namespace risc;

// Demo: fill array[0..4] = 1..5 at 0x4000, call sum() to add them up, store the result at 0x4014.
const std::vector<isa::Word> PROGRAM = {
    0x84404000, // 0x00 main:      mov  r1, 0x4000     ; r1 = array pointer
    0x84800001, // 0x04            mov  r2, 1          ; r2 = next value
    0x94840000, // 0x08 fill:      st   r2, 0[r1]      ; array[i] = value
    0x04440004, // 0x0C            add  r1, r1, 4
    0x04880001, // 0x10            add  r2, r2, 1
    0x2C080006, // 0x14            cmp  r2, 6
    0xC0000002, // 0x18            beq  done_fill      ; +2 -> 0x20
    0xE7FFFFFB, // 0x1C            b    fill           ; -5 -> 0x08
    0xE8000003, // 0x20 done_fill: call sum            ; +3 -> 0x2C, ra = 0x24
    0x94C40000, // 0x24            st   r3, 0[r1]      ; mem[0x4014] = sum
    0xA0000000, // 0x28            hlt
    0x84C00000, // 0x2C sum:       mov  r3, 0          ; r3 = sum
    0x85004000, // 0x30            mov  r4, 0x4000     ; r4 = pointer
    0x8D500000, // 0x34 sum_loop:  ld   r5, 0[r4]
    0x00CD4000, // 0x38            add  r3, r3, r5
    0x05100004, // 0x3C            add  r4, r4, 4
    0x28104000, // 0x40            cmp  r4, r1         ; reached the end (r1)?
    0xC0000002, // 0x44            beq  sum_done       ; +2 -> 0x4C
    0xE7FFFFFB, // 0x48            b    sum_loop       ; -5 -> 0x34
    0xF0000000, // 0x4C sum_done:  ret                 ; back to 0x24
};

// Usage: risc_run [clock period in ms]   (default 500 ms per cycle)
int main(int argc, char* argv[]) {
    int clock_period_ms = 500;
    if (argc > 1) {
        clock_period_ms = std::atoi(argv[1]);
    }

    memory::Memory memory;

    // Loader: put the program's encodings into TEXT
    for (unsigned i = 0; i < PROGRAM.size(); i++) {
        memory.load_word(machine::INSTRUCTION_BASE + i * isa::WORD_BYTES, PROGRAM[i]);
    }

    cpu::CPU cpu(memory);
    visualizer::Visualizer visualizer;

    // Synthetic slow clock: one cycle, redraw, wait
    visualizer.draw(cpu, memory);
    while (cpu.status == cpu::CpuStatus::Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(clock_period_ms));
        cpu.step();
        visualizer.draw(cpu, memory);
    }
}
