#include <cstdio>

#include "risc/isa/isa_config.hpp"
#include "risc/isa/machine_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/control/micro_control_unit.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/micro/datapath/datapath.hpp"

using namespace risc;
using namespace risc::micro;

const char* MICRO_OP_NAMES[] = {"mloadIR", "mdecode", "mswitch", "mmov", "mmovi", "madd", "mbeq", "mb"};

int main() {
    memory::Memory memory;
    memory.write_instruction_word(machine::INSTRUCTION_BASE, 0x84404000); // mov r1, 0x4000

    Datapath datapath;
    MicroControlUnit control;

    // The preamble every instruction starts with (§9.6.3)
    control.load({
        mloadIR(), // µ0: ir <- [pc]
        mdecode(), // µ1: fill the decode registers
        madd(MicroReg::pc, 4), // µ2: pc <- pc + 4
        mswitch(), // µ3: jump to the instruction's microcode
    });

    for (int cycle = 1; cycle <= 4; cycle++) {
        unsigned upc = control.upc;
        MicroWord word = control.microprogram[upc];
        Microinstruction m = MicroDecodeUnit::fields(word);

        control.step(datapath, memory);

        std::printf("cycle %d  µpc=%u  %-8s word=0x%012llX  ->  next µpc=%u\n",
                    cycle, upc, MICRO_OP_NAMES[unsigned(m.type)], (unsigned long long)word, control.upc);
        std::printf("    pc=0x%04X ir=0x%08X | I=%d rd=%u rs1=%u rs2=%u immx=0x%X branchTarget=0x%X | write bus=0x%X read bus=0x%X\n",
                    datapath.fetch.pc, datapath.fetch.ir,
                    datapath.decode.I, datapath.decode.rd, datapath.decode.rs1, datapath.decode.rs2,
                    datapath.decode.immx, datapath.decode.branchTarget,
                    datapath.write_bus, datapath.read_bus);
    }
}
