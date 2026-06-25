#pragma once

#include "hasher.hpp"

namespace devforge {
namespace crypto {

class SecureCipher {
public:
    explicit SecureCipher(std::string_view key);

    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext) const;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext) const;
    std::vector<uint8_t> derive_key(std::string_view password, std::string_view salt) const;
    std::vector<uint8_t> generate_iv() const;

    template<typename Container>
    bool validate(const Container& plaintext, const Container& ciphertext) const {
        auto decrypted = decrypt(ciphertext);
        return decrypted == plaintext;
    }

private:
    Cipher cipher_;
    Hasher hasher_;
};

} // namespace crypto
} // namespace devforge
