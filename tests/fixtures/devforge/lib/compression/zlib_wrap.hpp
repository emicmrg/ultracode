#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <stdexcept>

namespace devforge {
namespace compression {

struct CompressedData {
    std::vector<uint8_t> data;
    size_t original_size = 0;
    uint32_t checksum = 0;
    int level = 6;
};

class ZlibWrapper {
public:
    explicit ZlibWrapper(int level = 6);

    CompressedData compress(const std::vector<uint8_t>& input) const;
    std::vector<uint8_t> decompress(const CompressedData& input) const;

    template<typename T>
    CompressedData compress_batch(const std::vector<T>& items) const {
        if (items.empty()) {
            throw std::invalid_argument("compress_batch: empty input");
        }
        std::vector<uint8_t> raw;
        raw.reserve(items.size() * sizeof(T));
        const auto* ptr = reinterpret_cast<const uint8_t*>(items.data());
        raw.assign(ptr, ptr + items.size() * sizeof(T));
        return compress(raw);
    }

    bool validate(const CompressedData& data) const;

private:
    int level_;
    uint32_t compute_checksum(const std::vector<uint8_t>& data) const;
};

} // namespace compression
} // namespace devforge
