#include "json.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace devforge {
namespace format {

JsonNode JsonParser::parse(std::string_view input) const {
    auto trimmed = input;
    skip_whitespace(trimmed);
    if (trimmed.empty()) {
        throw std::invalid_argument("parse: empty input");
    }
    return parse_value(trimmed);
}

std::string JsonParser::serialize(const JsonNode& node, bool pretty) const {
    return serialize_impl(node, 0, pretty);
}

bool JsonParser::validate(const JsonNode& node, const std::string& expected_type) const {
    if (expected_type == "object") return std::holds_alternative<std::map<std::string, JsonNode>>(node.value);
    if (expected_type == "array")  return std::holds_alternative<std::vector<JsonNode>>(node.value);
    if (expected_type == "string") return std::holds_alternative<std::string>(node.value);
    if (expected_type == "number") return std::holds_alternative<int64_t>(node.value) || std::holds_alternative<double>(node.value);
    if (expected_type == "bool")   return std::holds_alternative<bool>(node.value);
    if (expected_type == "null")   return std::holds_alternative<std::nullptr_t>(node.value);
    return false;
}

JsonNode JsonParser::parse_value(std::string_view& input) const {
    skip_whitespace(input);
    if (input.empty()) {
        throw std::invalid_argument("parse_value: unexpected end");
    }
    switch (input[0]) {
        case '{': return parse_object(input);
        case '[': return parse_array(input);
        case '"': return parse_string(input);
        case 't': case 'f': case 'n': return parse_bool_null(input);
        default:  return parse_number(input);
    }
}

JsonNode JsonParser::parse_object(std::string_view& input) const {
    input.remove_prefix(1);
    std::map<std::string, JsonNode> obj;
    skip_whitespace(input);
    if (input.empty() || input[0] == '}') {
        if (!input.empty()) input.remove_prefix(1);
        return JsonNode(obj);
    }
    while (true) {
        skip_whitespace(input);
        auto key_node = parse_string(input);
        auto key = std::get<std::string>(key_node.value);
        skip_whitespace(input);
        if (input.empty() || input[0] != ':') {
            throw std::invalid_argument("parse_object: expected ':'");
        }
        input.remove_prefix(1);
        auto value = parse_value(input);
        obj[key] = value;
        skip_whitespace(input);
        if (input.empty()) {
            throw std::invalid_argument("parse_object: unexpected end");
        }
        if (input[0] == '}') {
            input.remove_prefix(1);
            break;
        }
        if (input[0] == ',') {
            input.remove_prefix(1);
        }
    }
    return JsonNode(obj);
}

JsonNode JsonParser::parse_array(std::string_view& input) const {
    input.remove_prefix(1);
    std::vector<JsonNode> arr;
    skip_whitespace(input);
    if (input.empty() || input[0] == ']') {
        if (!input.empty()) input.remove_prefix(1);
        return JsonNode(arr);
    }
    while (true) {
        arr.push_back(parse_value(input));
        skip_whitespace(input);
        if (input.empty()) {
            throw std::invalid_argument("parse_array: unexpected end");
        }
        if (input[0] == ']') {
            input.remove_prefix(1);
            break;
        }
        if (input[0] == ',') {
            input.remove_prefix(1);
        }
    }
    return JsonNode(arr);
}

JsonNode JsonParser::parse_string(std::string_view& input) const {
    input.remove_prefix(1);
    std::string result;
    while (!input.empty()) {
        char c = input[0];
        input.remove_prefix(1);
        if (c == '"') break;
        if (c == '\\' && !input.empty()) {
            char n = input[0];
            input.remove_prefix(1);
            switch (n) {
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                default:   result += n;    break;
            }
        } else {
            result += c;
        }
    }
    return JsonNode(result);
}

JsonNode JsonParser::parse_number(std::string_view& input) const {
    size_t i = 0;
    bool is_double = false;
    while (i < input.size() && (std::isdigit(input[i]) || input[i] == '-' || input[i] == '.' || input[i] == 'e' || input[i] == 'E' || input[i] == '+')) {
        if (input[i] == '.' || input[i] == 'e' || input[i] == 'E') is_double = true;
        ++i;
    }
    std::string num(input.data(), i);
    input.remove_prefix(i);
    if (is_double) {
        return JsonNode(std::stod(num));
    }
    return JsonNode(static_cast<int64_t>(std::stoll(num)));
}

JsonNode JsonParser::parse_bool_null(std::string_view& input) const {
    if (input.rfind("true", 0) == 0)  { input.remove_prefix(4); return JsonNode(true); }
    if (input.rfind("false", 0) == 0) { input.remove_prefix(5); return JsonNode(false); }
    if (input.rfind("null", 0) == 0)  { input.remove_prefix(4); return JsonNode(nullptr); }
    throw std::invalid_argument("parse_bool_null: unexpected token");
}

void JsonParser::skip_whitespace(std::string_view& input) const {
    while (!input.empty() && (input[0] == ' ' || input[0] == '\t' || input[0] == '\n' || input[0] == '\r')) {
        input.remove_prefix(1);
    }
}

std::string JsonParser::serialize_impl(const JsonNode& node, int indent, bool pretty) const {
    std::ostringstream oss;
    auto pad = [&](int extra) {
        if (pretty) oss << std::string(indent + extra, ' ');
    };

    if (std::holds_alternative<std::nullptr_t>(node.value)) {
        oss << "null";
    } else if (auto* b = std::get_if<bool>(&node.value)) {
        oss << (*b ? "true" : "false");
    } else if (auto* i = std::get_if<int64_t>(&node.value)) {
        oss << *i;
    } else if (auto* d = std::get_if<double>(&node.value)) {
        oss << *d;
    } else if (auto* s = std::get_if<std::string>(&node.value)) {
        oss << '"' << escape_json_string(*s) << '"';
    } else if (auto* arr = std::get_if<std::vector<JsonNode>>(&node.value)) {
        oss << '[';
        if (pretty && !arr->empty()) oss << '\n';
        for (size_t i = 0; i < arr->size(); ++i) {
            pad(2);
            oss << serialize_impl((*arr)[i], indent + 2, pretty);
            if (i + 1 < arr->size()) oss << ',';
            if (pretty) oss << '\n';
        }
        if (pretty && !arr->empty()) pad(0);
        oss << ']';
    } else if (auto* obj = std::get_if<std::map<std::string, JsonNode>>(&node.value)) {
        oss << '{';
        if (pretty && !obj->empty()) oss << '\n';
        size_t count = 0;
        for (const auto& [k, v] : *obj) {
            pad(2);
            oss << '"' << escape_json_string(k) << "\": ";
            oss << serialize_impl(v, indent + 2, pretty);
            if (++count < obj->size()) oss << ',';
            if (pretty) oss << '\n';
        }
        if (pretty && !obj->empty()) pad(0);
        oss << '}';
    }
    return oss.str();
}

std::string JsonParser::escape_json_string(std::string_view s) const {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:   oss << c;
        }
    }
    return oss.str();
}

} // namespace format
} // namespace devforge
