#pragma once

#include <string>
#include <vector>

#include "compare.hpp"
#include "nlohmann/json.hpp"

namespace risc::web {

using nlohmann::json;

// Delay, area and µcycles of every adder, multiplier and divider, counted from their gates
json alu_units_json();

// The programs on all three processors with the default ALU, then again with each other algorithm
// (one unit changed at a time): total µcycles and each program's µcycles
json alu_runs_json(const Comparer& comparer, const std::vector<std::string>& sources);
}
