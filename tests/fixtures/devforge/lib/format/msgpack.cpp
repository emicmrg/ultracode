#include "msgpack.hpp"
#include <cstring>

namespace devforge {
namespace format {

std::vector<uint8_t> MessagePackParser::pack(const MessagePackNode& node) const {
    std::vector<uint8_t> result;
    pack_value(result, node);
    return result;
}

MessagePackNode MessagePackParser::unpack(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        throw std::invalid_argument("unpack: empty data");
    }
    const uint8_t* ptr = data.data();
    return unpack_value(ptr, data.data() + data.size());
}

bool MessagePackParser::validate(const std::vector<uint8_t>& data) const {
    try {
        unpack(data);
        return true;
    } catch (...) {
        return false;
    }
}

void MessagePackParser::pack_value(std::vector<uint8_t>& out, const MessagePackNode& node) const {
    if (std::holds_alternative<std::nullptr_t>(node.value)) {
        out.push_back(0xc0);
    } else if (auto* b = std::get_if<bool>(&node.value)) {
        out.push_back(*b ? 0xc3 : 0xc2);
    } else if (auto* i = std::get_if<int64_t>(&node.value)) {
        write_int(out, *i);
    } else if (auto* d = std::get_if<double>(&node.value)) {
        out.push_back(0xcb);
        uint64_t bits;
        std::memcpy(&bits, &(*d), sizeof(bits));
        for (int i = 7; i >= 0; --i) out.push_back(static_cast<uint8_t>(bits >> (i * 8)));
    } else if (auto* s = std::get_if<std::string>(&node.value)) {
        write_str(out, *s);
    } else if (auto* arr = std::get_if<std::vector<MessagePackNode>>(&node.value)) {
        if (arr->size() < 16) out.push_back(0x90 | static_cast<uint8_t>(arr->size()));
        else { out.push_back(0xdc); out.push_back(static_cast<uint8_t>(arr->size() >> 8)); out.push_back(static_cast<uint8_t>(arr->size())); }
        for (const auto& item : *arr) pack_value(out, item);
    } else if (auto* obj = std::get_if<std::map<std::string, MessagePackNode>>(&node.value)) {
        if (obj->size() < 16) out.push_back(0x80 | static_cast<uint8_t>(obj->size()));
        else { out.push_back(0xde); out.push_back(static_cast<uint8_t>(obj->size() >> 8)); out.push_back(static_cast<uint8_t>(obj->size())); }
        for (const auto& [k, v] : *obj) { write_str(out, k); pack_value(out, v); }
    }
}

MessagePackNode MessagePackParser::unpack_value(const uint8_t*& ptr, const uint8_t* end) const {
    if (ptr >= end) throw std::invalid_argument("unpack_value: unexpected end");
    uint8_t tag = *ptr++;
    switch (tag) {
        case 0xc0: return MessagePackNode(nullptr);
        case 0xc2: return MessagePackNode(false);
        case 0xc3: return MessagePackNode(true);
        case 0xcc: { uint8_t v = *ptr++; return MessagePackNode(static_cast<int64_t>(v)); }
        case 0xcd: { uint16_t v = (*ptr++ << 8) | *ptr++; return MessagePackNode(static_cast<int64_t>(v)); }
        case 0xce: { uint32_t v = (*ptr++ << 24) | (*ptr++ << 16) | (*ptr++ << 8) | *ptr++; return MessagePackNode(static_cast<int64_t>(v)); }
        default: {
            if (tag >= 0xa0 && tag <= 0xbf) {
                size_t len = tag & 0x1f;
                std::string s(reinterpret_cast<const char*>(ptr), len);
                ptr += len;
                return MessagePackNode(s);
            }
            if (tag >= 0x90 && tag <= 0x9f) {
                size_t len = tag & 0x0f;
                std::vector<MessagePackNode> arr;
                for (size_t i = 0; i < len; ++i) arr.push_back(unpack_value(ptr, end));
                return MessagePackNode(arr);
            }
            if (tag >= 0x80 && tag <= 0x8f) {
                size_t len = tag & 0x0f;
                std::map<std::string, MessagePackNode> obj;
                for (size_t i = 0; i < len; ++i) {
                    auto key = unpack_value(ptr, end);
                    auto val = unpack_value(ptr, end);
                    if (auto* ks = std::get_if<std::string>(&key.value)) obj[*ks] = val;
                }
                return MessagePackNode(obj);
            }
        }
    }
    throw std::invalid_argument("unpack_value: unknown tag");
}

void MessagePackParser::write_int(std::vector<uint8_t>& out, int64_t value) const {
    if (value >= 0 && value <= 127) {
        out.push_back(static_cast<uint8_t>(value));
    } else if (value >= -32 && value < 0) {
        out.push_back(static_cast<uint8_t>(value));
    } else if (value >= -128 && value <= 127) {
        out.push_back(0xd0); out.push_back(static_cast<uint8_t>(value));
    } else if (value >= -32768 && value <= 32767) {
        out.push_back(0xd1); out.push_back(static_cast<uint8_t>(value >> 8)); out.push_back(static_cast<uint8_t>(value));
    } else if (value >= -2147483648LL && value <= 2147483647LL) {
        out.push_back(0xd2);
        for (int i = 3; i >= 0; --i) out.push_back(static_cast<uint8_t>(value >> (i * 8)));
    } else {
        out.push_back(0xd3);
        for (int i = 7; i >= 0; --i) out.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
}

void MessagePackParser::write_str(std::vector<uint8_t>& out, std::string_view value) const {
    if (value.size() < 32) out.push_back(0xa0 | static_cast<uint8_t>(value.size()));
    else if (value.size() < 256) { out.push_back(0xd9); out.push_back(static_cast<uint8_t>(value.size())); }
    else { out.push_back(0xda); out.push_back(static_cast<uint8_t>(value.size() >> 8)); out.push_back(static_cast<uint8_t>(value.size())); }
    out.insert(out.end(), value.begin(), value.end());
}

void MessagePackParser::write_bin(std::vector<uint8_t>& out, const std::vector<uint8_t>& data) const {
    if (data.size() < 256) { out.push_back(0xc4); out.push_back(static_cast<uint8_t>(data.size())); }
    else { out.push_back(0xc5); out.push_back(static_cast<uint8_t>(data.size() >> 8)); out.push_back(static_cast<uint8_t>(data.size())); }
    out.insert(out.end(), data.begin(), data.end());
}

} // namespace format
} // namespace devforge
