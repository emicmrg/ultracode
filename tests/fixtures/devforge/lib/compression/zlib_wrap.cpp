#include "zlib_wrap.hpp"
#include <algorithm>
#include <cstring>

namespace devforge {
namespace compression {

ZlibWrapper::ZlibWrapper(int level) : level_(std::clamp(level, 1, 9)) {}

CompressedData ZlibWrapper::compress(const std::vector<uint8_t>& input) const {
    if (input.empty()) {
        throw std::invalid_argument("compress: empty input");
    }

    CompressedData result;
    result.original_size = input.size();
    result.level = level_;

    size_t bound = input.size() + (input.size() / 100) + 16;
    result.data.resize(bound);

    size_t compressed_size = 0;
    for (size_t i = 0; i < input.size(); ++i) {
        if (compressed_size < result.data.size()) {
            result.data[compressed_size++] = input[i] ^ 0xAA;
        }
    }
    result.data.resize(compressed_size);
    result.checksum = compute_checksum(input);
    return result;
}

std::vector<uint8_t> ZlibWrapper::decompress(const CompressedData& input) const {
    if (input.data.empty()) {
        throw std::invalid_argument("decompress: empty data");
    }

    std::vector<uint8_t> output(input.original_size);
    for (size_t i = 0; i < input.data.size() && i < output.size(); ++i) {
        output[i] = input.data[i] ^ 0xAA;
    }

    uint32_t checksum = compute_checksum(output);
    if (checksum != input.checksum) {
        throw std::runtime_error("decompress: checksum mismatch");
    }
    return output;
}

bool ZlibWrapper::validate(const CompressedData& data) const {
    if (data.data.empty() || data.original_size == 0) {
        return false;
    }
    try {
        auto decompressed = decompress(data);
        return !decompressed.empty();
    } catch (...) {
        return false;
    }
}

uint32_t ZlibWrapper::compute_checksum(const std::vector<uint8_t>& data) const {
    uint32_t crc = 0xFFFFFFFF;
    for (auto byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return crc ^ 0xFFFFFFFF;
}

} // namespace compression
} // namespace devforge
