#pragma once

#include <set>
#include <string>

#include "nlohmann/json.hpp"
#include "risc/alu/alu_types.hpp"
#include "risc/exception.hpp"
#include "risc/isa/isa_config.hpp"
#include "risc/memory/memory.hpp"
#include "risc/micro/control/micro_decode_unit.hpp"
#include "risc/micro/control/microinstruction.hpp"
#include "risc/micro/micro_isa.hpp"

// JSON pieces shared by the six-stage and the four-stage sessions
namespace risc::web {

using nlohmann::json;

std::string register_name(micro::MicroReg reg);

// The text of an args field, e.g. "add", "read", "load", "halt" ("" for no args)
std::string args_name(micro::Args a);

// The unit an args field goes to, e.g. "alu" ("" for no args)
std::string args_unit_name(micro::Args a);

std::string exception_name(const CpuException& e);

std::string trim(const std::string& text);

std::string disassemble(isa::Word pc, isa::Word ir);

json microinstruction_json(micro::MicroWord word);

json signals_json(const micro::MicroControlSignals& s);

// {"adder": "prefix", "multiplier": "booth4", "divider": "nonrestoring"}
json alu_config_json(const alu::AluConfig& config);

// The other way; throws std::runtime_error for an unknown name
alu::AluConfig alu_config_from_json(const json& j);

// The stack region around sp: sp, the bounds, and the words from just below sp up to the top.
// return_slots: the addresses whose last store came from ra (r15), i.e. saved return addresses.
json stack_json(const memory::Memory& memory, isa::Word sp, const std::set<isa::Word>& return_slots);

// The memory address a st instruction writes and whether its data register is ra, from its packet
// once EX has computed the address. Returns false if the packet is not a st.
template <typename Packet>
bool store_of(const Packet& p, isa::Word& address, bool& from_ra) {
    unsigned opcode = (p.ir >> isa::OPCODE_POS) & ((1u << isa::OPCODE_BITS) - 1);
    if (p.bubble || p.hasException || isa::Opcode(opcode) != isa::Opcode::ST) {
        return false;
    }
    address = p.aluResult;
    from_ra = p.rd == isa::REG_RA;
    return true;
}

// An instruction packet of either pipeline (they have the same fields)
template <typename Packet>
json packet_json(const Packet& p) {
    json j;
    j["bubble"] = p.bubble;
    j["id"] = p.id;
    j["currentPc"] = p.currentPc;
    j["nextPc"] = p.nextPc;
    j["ir"] = p.ir;
    j["assembly"] = "";
    if (!p.bubble && !p.hasException) {
        j["assembly"] = disassemble(p.currentPc, p.ir);
    }
    j["I"] = p.I;
    j["rd"] = p.rd;
    j["rs1"] = p.rs1;
    j["rs2"] = p.rs2;
    j["immx"] = p.immx;
    j["branchTarget"] = p.branchTarget;
    j["A"] = p.A;
    j["B"] = p.B;
    j["regVal"] = p.regVal;
    j["regData"] = p.regData;
    j["aluResult"] = p.aluResult;
    j["ldResult"] = p.ldResult;
    j["hasException"] = p.hasException;
    j["exception"] = "";
    if (p.hasException) {
        j["exception"] = exception_name(p.exception);
    }
    return j;
}
}
