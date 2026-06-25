#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <stdexcept>

namespace devforge {
namespace format {

using JsonValue = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<struct JsonNode>,
    std::map<std::string, struct JsonNode>
>;

struct JsonNode {
    JsonValue value;

    JsonNode() : value(nullptr) {}
    JsonNode(JsonValue v) : value(std::move(v)) {}

    template<typename T>
    std::optional<T> get() const {
        if (auto* p = std::get_if<T>(&value)) {
            return *p;
        }
        return std::nullopt;
    }
};

class JsonParser {
public:
    JsonParser() = default;

    JsonNode parse(std::string_view input) const;
    std::string serialize(const JsonNode& node, bool pretty = false) const;

    template<typename T>
    bool validate(const JsonNode& node) const {
        return std::holds_alternative<T>(node.value);
    }

    bool validate(const JsonNode& node, const std::string& expected_type) const;

private:
    JsonNode parse_value(std::string_view& input) const;
    JsonNode parse_object(std::string_view& input) const;
    JsonNode parse_array(std::string_view& input) const;
    JsonNode parse_string(std::string_view& input) const;
    JsonNode parse_number(std::string_view& input) const;
    JsonNode parse_bool_null(std::string_view& input) const;

    void skip_whitespace(std::string_view& input) const;

    std::string serialize_impl(const JsonNode& node, int indent, bool pretty) const;
    std::string escape_json_string(std::string_view s) const;
};

} // namespace format
} // namespace devforge
