#pragma once

#include <string>

#include "risc/alu/alu_types.hpp"

namespace risc::alu {

// Command-line names: --adder ripple|select|cla|prefix  --mul shiftadd|booth2|booth4  --div restoring|nonrestoring
inline const char* ALU_OPTIONS_USAGE = "[--adder ripple|select|cla|prefix] [--mul shiftadd|booth2|booth4] [--div restoring|nonrestoring]";

// Reads one option (e.g. "--adder", "ripple") into config. False if the option or the value is unknown.
inline bool read_alu_option(const std::string& option, const std::string& value, AluConfig& config) {
    if (option == "--adder") {
        if (value == "ripple") {
            config.adder = AdderAlgorithm::RippleCarry;
        } else if (value == "select") {
            config.adder = AdderAlgorithm::CarrySelect;
        } else if (value == "cla") {
            config.adder = AdderAlgorithm::HierarchicalGroupedCLA;
        } else if (value == "prefix") {
            config.adder = AdderAlgorithm::PrefixTreeCLA;
        } else {
            return false;
        }
        return true;
    }
    if (option == "--mul") {
        if (value == "shiftadd") {
            config.multiplier = MultiplierAlgorithm::ShiftAdd;
        } else if (value == "booth2") {
            config.multiplier = MultiplierAlgorithm::BoothRadix2;
        } else if (value == "booth4") {
            config.multiplier = MultiplierAlgorithm::BoothRadix4;
        } else {
            return false;
        }
        return true;
    }
    if (option == "--div") {
        if (value == "restoring") {
            config.divider = DividerAlgorithm::Restoring;
        } else if (value == "nonrestoring") {
            config.divider = DividerAlgorithm::NonRestoring;
        } else {
            return false;
        }
        return true;
    }
    return false;
}

// The command-line name of each algorithm (the inverse of read_alu_option)
inline const char* adder_key(AdderAlgorithm algorithm) {
    switch (algorithm) {
        case AdderAlgorithm::RippleCarry:
            return "ripple";
        case AdderAlgorithm::CarrySelect:
            return "select";
        case AdderAlgorithm::HierarchicalGroupedCLA:
            return "cla";
        case AdderAlgorithm::PrefixTreeCLA:
            return "prefix";
    }
    return "";
}

inline const char* multiplier_key(MultiplierAlgorithm algorithm) {
    switch (algorithm) {
        case MultiplierAlgorithm::ShiftAdd:
            return "shiftadd";
        case MultiplierAlgorithm::BoothRadix2:
            return "booth2";
        case MultiplierAlgorithm::BoothRadix4:
            return "booth4";
    }
    return "";
}

inline const char* divider_key(DividerAlgorithm algorithm) {
    if (algorithm == DividerAlgorithm::Restoring) {
        return "restoring";
    }
    return "nonrestoring";
}
}
