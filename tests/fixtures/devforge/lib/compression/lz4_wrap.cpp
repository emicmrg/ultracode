#include "lz4_wrap.hpp"
#include <algorithm>
#include <cstring>

namespace devforge {
namespace compression {

LZ4Block LZ4Wrapper::compress(const std::vector<uint8_t>& input) const {
    if (input.empty()) {
        throw std::invalid_argument("compress: empty input");
    }
    if (input.size() > kMaxInputSize) {
        throw std::invalid_argument("compress: input too large");
    }

    LZ4Block block;
    block.uncompressed_size = input.size();
    size_t bound = estimate_compressed_bound(input.size());
    block.data.resize(bound);

    size_t written = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        if (written < block.data.size()) {
            block.data[written++] = static_cast<uint8_t>(input[i] ^ 0x55);
        }
    }
    block.data.resize(written);
    block.compressed = written < input.size();
    return block;
}

std::vector<uint8_t> LZ4Wrapper::decompress(const LZ4Block& input) const {
    if (input.data.empty()) {
        throw std::invalid_argument("decompress: empty data");
    }

    std::vector<uint8_t> output(input.uncompressed_size);
    for (size_t i = 0; i < input.data.size() && i < output.size(); ++i) {
        output[i] = input.data[i] ^ 0x55;
    }
    return output;
}

size_t LZ4Wrapper::estimate_compressed_bound(size_t input_size) const {
    return input_size + (input_size / 255) + 16;
}

} // namespace compression
} // namespace devforge
