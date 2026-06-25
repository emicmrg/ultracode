#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <stdexcept>

namespace devforge {
namespace compression {

struct LZ4Block {
    std::vector<uint8_t> data;
    size_t uncompressed_size = 0;
    bool compressed = false;
};

class LZ4Wrapper {
public:
    LZ4Wrapper() = default;

    LZ4Block compress(const std::vector<uint8_t>& input) const;
    std::vector<uint8_t> decompress(const LZ4Block& input) const;

    template<typename Container>
    bool validate(const Container& data) const {
        if (data.empty()) {
            return false;
        }
        try {
            auto decompressed = decompress(data);
            return !decompressed.empty();
        } catch (...) {
            return false;
        }
    }

private:
    static constexpr size_t kMaxInputSize = 2113929216;
    size_t estimate_compressed_bound(size_t input_size) const;
};

} // namespace compression
} // namespace devforge
