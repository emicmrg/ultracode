#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <variant>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace devforge {
namespace format {

using MessagePackValue = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<uint8_t>,
    std::vector<struct MessagePackNode>,
    std::map<std::string, struct MessagePackNode>
>;

struct MessagePackNode {
    MessagePackValue value;

    MessagePackNode() : value(nullptr) {}
    MessagePackNode(MessagePackValue v) : value(std::move(v)) {}

    template<typename T>
    std::optional<T> get() const {
        if (auto* p = std::get_if<T>(&value)) {
            return *p;
        }
        return std::nullopt;
    }
};

class MessagePackParser {
public:
    MessagePackParser() = default;

    std::vector<uint8_t> pack(const MessagePackNode& node) const;
    MessagePackNode unpack(const std::vector<uint8_t>& data) const;

    template<typename T>
    bool validate(const MessagePackNode& node) const {
        return std::holds_alternative<T>(node.value);
    }

    bool validate(const std::vector<uint8_t>& data) const;

private:
    void pack_value(std::vector<uint8_t>& out, const MessagePackNode& node) const;
    MessagePackNode unpack_value(const uint8_t*& ptr, const uint8_t* end) const;

    void write_int(std::vector<uint8_t>& out, int64_t value) const;
    void write_str(std::vector<uint8_t>& out, std::string_view value) const;
    void write_bin(std::vector<uint8_t>& out, const std::vector<uint8_t>& data) const;
};

} // namespace format
} // namespace devforge
