#pragma once

#include <cmath>
#include <string_view>

namespace kc868a6::core {
struct ConditionState {
    bool matched = false;
    bool seen = false;
    double previous = 0;

    bool fires(double value, double threshold, std::string_view comparison) {
        if (!std::isfinite(value)) {
            matched = false;
            seen = false;
            return false;
        }
        const bool changed = comparison == "changed";
        const bool next = changed ? seen && value != previous :
            comparison == ">" ? value > threshold :
            comparison == "<" ? value < threshold :
            comparison == ">=" ? value >= threshold :
            comparison == "<=" ? value <= threshold :
            comparison == "==" ? value == threshold :
            comparison == "!=" ? value != threshold : false;
        const bool fire = next && (changed || !matched);
        previous = value;
        seen = true;
        matched = next;
        return fire;
    }
};
}
