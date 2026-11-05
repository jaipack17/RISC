#include "alu_report.hpp"

#include "json_util.hpp"
#include "risc/alu/alu.hpp"
#include "risc/alu/alu_options.hpp"

namespace risc::web {

namespace {

const alu::AdderAlgorithm ADDERS[] = {alu::AdderAlgorithm::RippleCarry, alu::AdderAlgorithm::CarrySelect,
                                      alu::AdderAlgorithm::HierarchicalGroupedCLA, alu::AdderAlgorithm::PrefixTreeCLA};
const alu::MultiplierAlgorithm MULTIPLIERS[] = {alu::MultiplierAlgorithm::ShiftAdd, alu::MultiplierAlgorithm::BoothRadix2,
                                                alu::MultiplierAlgorithm::BoothRadix4};
const alu::DividerAlgorithm DIVIDERS[] = {alu::DividerAlgorithm::Restoring, alu::DividerAlgorithm::NonRestoring};

json iterative_json(const alu::IterativeCharacteristics& c) {
    json j;
    j["stepDelay"] = c.step_delay;
    j["area"] = c.area;
    j["min"] = c.min_microcycles;
    j["average"] = c.average_microcycles;
    j["max"] = c.max_microcycles;
    return j;
}

// One processor's µcycles over all the programs
struct ProcessorTotal {
    std::uint64_t microcycles = 0;
    std::uint64_t instructions = 0;
    json per_program = json::array();

    void add(const json& run) {
        microcycles += run["microcycles"].get<std::uint64_t>();
        instructions += run["instructions"].get<std::uint64_t>();
        per_program.push_back(run["microcycles"]);
    }

    json to_json() const {
        json j;
        j["microcycles"] = microcycles;
        j["instructions"] = instructions;
        j["perProgram"] = per_program;
        return j;
    }
};

json run_all(const Comparer& comparer, const std::vector<std::string>& sources, const std::string& unit,
             const alu::AluConfig& config) {
    ProcessorTotal four;
    ProcessorTotal six;
    ProcessorTotal sequential;
    unsigned agree = 0;
    for (const std::string& source : sources) {
        json result = comparer.compare(source, false, config);
        four.add(result["four"]);
        six.add(result["six"]);
        sequential.add(result["sequential"]);
        if (result["agree"].get<bool>()) {
            agree++;
        }
    }
    json j;
    j["unit"] = unit; // "default", or the unit that differs from it
    j["alu"] = alu_config_json(config);
    j["four"] = four.to_json();
    j["six"] = six.to_json();
    j["sequential"] = sequential.to_json();
    j["agree"] = agree;
    return j;
}
} // namespace

json alu_units_json() {
    json j;
    j["microcycleLength"] = alu::microcycle_length();
    j["fastestAdder"] = alu::adder_key(alu::fastest_adder());
    j["defaults"] = alu_config_json(alu::AluConfig{});

    j["adders"] = json::array();
    for (alu::AdderAlgorithm adder : ADDERS) {
        alu::AdderCharacteristics c = alu::measure_adder(adder);
        json a;
        a["key"] = alu::adder_key(adder);
        a["name"] = c.name;
        a["delay"] = c.delay;
        a["area"] = c.area;
        a["compareDelay"] = c.compare_delay;
        a["addMicrocycles"] = c.add_microcycles;
        a["compareMicrocycles"] = c.compare_microcycles;
        j["adders"].push_back(a);
    }

    // Each multiplier and divider with each adder (every step is one pass through the adder)
    j["multipliers"] = json::array();
    for (alu::MultiplierAlgorithm multiplier : MULTIPLIERS) {
        json m;
        m["key"] = alu::multiplier_key(multiplier);
        m["name"] = alu::Multipliers::name(multiplier);
        for (alu::AdderAlgorithm adder : ADDERS) {
            m["byAdder"][alu::adder_key(adder)] = iterative_json(alu::measure_multiplier(multiplier, adder));
        }
        j["multipliers"].push_back(m);
    }
    j["dividers"] = json::array();
    for (alu::DividerAlgorithm divider : DIVIDERS) {
        json d;
        d["key"] = alu::divider_key(divider);
        d["name"] = alu::Dividers::name(divider);
        for (alu::AdderAlgorithm adder : ADDERS) {
            d["byAdder"][alu::adder_key(adder)] = iterative_json(alu::measure_divider(divider, adder));
        }
        j["dividers"].push_back(d);
    }
    return j;
}

json alu_runs_json(const Comparer& comparer, const std::vector<std::string>& sources) {
    const alu::AluConfig defaults;
    json runs = json::array();
    runs.push_back(run_all(comparer, sources, "default", defaults));
    for (alu::AdderAlgorithm adder : ADDERS) {
        if (adder != defaults.adder) {
            alu::AluConfig config = defaults;
            config.adder = adder;
            runs.push_back(run_all(comparer, sources, "adder", config));
        }
    }
    for (alu::MultiplierAlgorithm multiplier : MULTIPLIERS) {
        if (multiplier != defaults.multiplier) {
            alu::AluConfig config = defaults;
            config.multiplier = multiplier;
            runs.push_back(run_all(comparer, sources, "multiplier", config));
        }
    }
    for (alu::DividerAlgorithm divider : DIVIDERS) {
        if (divider != defaults.divider) {
            alu::AluConfig config = defaults;
            config.divider = divider;
            runs.push_back(run_all(comparer, sources, "divider", config));
        }
    }
    return runs;
}
}
