#pragma once

#include <string>
#include <vector>

namespace ultracode {

struct PlanStep {
    std::string file;
    std::string reason;
    std::vector<std::string> changes;
};

struct PlanResult {
    std::vector<PlanStep> steps;
    std::vector<std::string> files_needed;
    bool valid = false;
    std::string error;
};

// Extract and parse a JSON plan from raw LLM output.
// The output may contain markdown fences or surrounding prose.
PlanResult parse_plan_from_response(const std::string& raw_response);

} // namespace ultracode
