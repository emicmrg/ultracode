#include "cipher.hpp"

namespace devforge {
namespace crypto {

SecureCipher::SecureCipher(std::string_view key) : cipher_(key) {}

std::vector<uint8_t> SecureCipher::encrypt(const std::vector<uint8_t>& plaintext) const {
    return cipher_.encrypt(plaintext);
}

std::vector<uint8_t> SecureCipher::decrypt(const std::vector<uint8_t>& ciphertext) const {
    return cipher_.decrypt(ciphertext);
}

std::vector<uint8_t> SecureCipher::derive_key(std::string_view password, std::string_view salt) const {
    return cipher_.derive_key(password, salt);
}

std::vector<uint8_t> SecureCipher::generate_iv() const {
    return cipher_.generate_iv();
}

} // namespace crypto
} // namespace devforge
