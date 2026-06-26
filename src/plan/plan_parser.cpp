#include "plan/plan_parser.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace ultracode {

namespace {

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

// Extract JSON block from text that may have markdown fences or prose.
std::string extract_json_block(const std::string& raw) {
    // Look for ```json ... ``` first
    size_t fence_start = raw.find("```json");
    if (fence_start != std::string::npos) {
        size_t content_start = raw.find('\n', fence_start);
        if (content_start != std::string::npos) {
            size_t fence_end = raw.find("```", content_start + 1);
            if (fence_end != std::string::npos) {
                return trim(raw.substr(content_start + 1, fence_end - content_start - 1));
            }
        }
    }
    // Look for ``` ... ``` without json tag
    fence_start = raw.find("```");
    if (fence_start != std::string::npos) {
        size_t content_start = raw.find('\n', fence_start);
        if (content_start != std::string::npos) {
            size_t fence_end = raw.find("```", content_start + 1);
            if (fence_end != std::string::npos) {
                std::string candidate = trim(raw.substr(content_start + 1, fence_end - content_start - 1));
                if (!candidate.empty() && candidate.front() == '{') {
                    return candidate;
                }
            }
        }
    }
    // Find first { and last }
    size_t brace_open = raw.find('{');
    if (brace_open == std::string::npos) return {};
    size_t brace_close = raw.rfind('}');
    if (brace_close == std::string::npos || brace_close <= brace_open) return {};
    return trim(raw.substr(brace_open, brace_close - brace_open + 1));
}

// Extract the content of a quoted string at position pos.
// Returns the string content and updates pos to after the closing quote.
std::pair<std::string, size_t> parse_json_string(const std::string& text, size_t pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    if (pos >= text.size() || text[pos] != '"') return {{}, pos};
    ++pos; // skip opening quote
    std::string result;
    while (pos < text.size() && text[pos] != '"') {
        if (text[pos] == '\\' && pos + 1 < text.size()) {
            ++pos; // skip backslash
        }
        result += text[pos];
        ++pos;
    }
    if (pos < text.size() && text[pos] == '"') ++pos; // skip closing quote
    return {result, pos};
}

// Parse a JSON array of strings: ["a", "b", "c"]
// Returns vector of strings. pos should point at '['.
std::vector<std::string> parse_string_array(const std::string& text, size_t pos) {
    std::vector<std::string> result;
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    if (pos >= text.size() || text[pos] != '[') return result;
    ++pos; // skip '['

    while (pos < text.size()) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
        if (pos < text.size() && text[pos] == ']') break;

        auto [str, next_pos] = parse_json_string(text, pos);
        if (!str.empty() || next_pos > pos) {
            result.push_back(str);
        }
        pos = next_pos;

        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
        if (pos < text.size() && text[pos] == ',') {
            ++pos;
        }
    }
    return result;
}

// Find the position after a specific key like "steps":
size_t find_key_position(const std::string& text, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = text.find(pattern);
    if (pos == std::string::npos) return std::string::npos;
    pos += pattern.size();
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    if (pos < text.size() && text[pos] == ':') ++pos;
    return pos;
}

// Extract the array block after a key (pos should point at '[')
std::string extract_array_block(const std::string& text, size_t pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    if (pos >= text.size() || text[pos] != '[') return {};

    int depth = 1;
    size_t start = pos;
    ++pos;
    while (pos < text.size() && depth > 0) {
        if (text[pos] == '"') {
            ++pos;
            while (pos < text.size() && text[pos] != '"') {
                if (text[pos] == '\\' && pos + 1 < text.size()) ++pos;
                ++pos;
            }
            if (pos < text.size()) ++pos;
        } else {
            if (text[pos] == '[') ++depth;
            else if (text[pos] == ']') --depth;
            ++pos;
        }
    }
    return text.substr(start, pos - start);
}

// Extract the object block starting at pos (pos should point at '{')
std::string extract_object_block(const std::string& text, size_t pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    if (pos >= text.size() || text[pos] != '{') return {};

    int depth = 1;
    size_t start = pos;
    ++pos;
    while (pos < text.size() && depth > 0) {
        if (text[pos] == '"') {
            ++pos;
            while (pos < text.size() && text[pos] != '"') {
                if (text[pos] == '\\' && pos + 1 < text.size()) ++pos;
                ++pos;
            }
            if (pos < text.size()) ++pos;
        } else {
            if (text[pos] == '{') ++depth;
            else if (text[pos] == '}') --depth;
            ++pos;
        }
    }
    return text.substr(start, pos - start);
}

// Parse a single step object
PlanStep parse_step_object(const std::string& obj_text) {
    PlanStep step;

    size_t file_pos = find_key_position(obj_text, "file");
    if (file_pos != std::string::npos) {
        auto [file_val, _] = parse_json_string(obj_text, file_pos);
        step.file = file_val;
    }

    size_t reason_pos = find_key_position(obj_text, "reason");
    if (reason_pos != std::string::npos) {
        auto [reason_val, _] = parse_json_string(obj_text, reason_pos);
        step.reason = reason_val;
    }

    size_t changes_pos = find_key_position(obj_text, "changes");
    if (changes_pos != std::string::npos) {
        step.changes = parse_string_array(obj_text, changes_pos);
    }

    return step;
}

} // namespace

PlanResult parse_plan_from_response(const std::string& raw_response) {
    PlanResult result;

    std::string json_text = extract_json_block(raw_response);
    if (json_text.empty()) {
        result.error = "No JSON block found in response";
        return result;
    }

    // Parse steps array
    size_t steps_pos = find_key_position(json_text, "steps");
    if (steps_pos != std::string::npos) {
        std::string steps_array = extract_array_block(json_text, steps_pos);
        if (!steps_array.empty()) {
            // Find individual objects in the array
            size_t pos = 0;
            while (pos < steps_array.size()) {
                size_t obj_start = steps_array.find('{', pos);
                if (obj_start == std::string::npos) break;
                std::string obj_text = extract_object_block(steps_array, obj_start);
                if (!obj_text.empty()) {
                    PlanStep step = parse_step_object(obj_text);
                    if (!step.file.empty()) {
                        result.steps.push_back(std::move(step));
                    }
                }
                pos = obj_start + obj_text.size();
            }
        }
    }

    // Parse files_needed array
    size_t files_pos = find_key_position(json_text, "files_needed");
    if (files_pos != std::string::npos) {
        result.files_needed = parse_string_array(json_text, files_pos);
    }

    result.valid = !result.steps.empty();
    if (!result.valid) {
        result.error = "No valid steps found in plan";
    }

    return result;
}

} // namespace ultracode
