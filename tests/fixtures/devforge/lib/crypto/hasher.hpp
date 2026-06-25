#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <array>

namespace devforge {
namespace crypto {

class Hasher {
public:
    Hasher() = default;

    std::string hash_sha256(std::string_view input) const;
    std::string hash_md5(std::string_view input) const;
    uint32_t hash_crc32(std::string_view input) const;

    template<typename T>
    std::string hash_any(const T& value) const {
        const auto* ptr = reinterpret_cast<const char*>(&value);
        std::string_view view(ptr, sizeof(T));
        return hash_sha256(view);
    }

    template<typename Container>
    bool validate(const Container& hash1, const Container& hash2) const {
        return hash1 == hash2;
    }

private:
    static constexpr uint32_t kSha256Init[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    std::string hex_encode(const std::vector<uint8_t>& data) const;
};

class Cipher {
public:
    explicit Cipher(std::string_view key);

    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext) const;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext) const;

    std::vector<uint8_t> generate_iv() const;
    std::vector<uint8_t> derive_key(std::string_view password, std::string_view salt) const;

    template<typename Container>
    bool validate(const Container& plaintext, const Container& ciphertext) const {
        auto decrypted = decrypt(ciphertext);
        return decrypted == plaintext;
    }

private:
    std::vector<uint8_t> key_;
    static constexpr size_t kBlockSize = 16;
    void xor_block(std::vector<uint8_t>& block, const std::vector<uint8_t>& mask) const;
};

} // namespace crypto
} // namespace devforge
