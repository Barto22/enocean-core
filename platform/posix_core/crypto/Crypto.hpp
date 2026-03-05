/**
 * @file Crypto.hpp
 * @brief POSIX OpenSSL implementation of ICrypto<AesCcm> for AES-128-CCM-4.
 *
 * Wraps the OpenSSL EVP AES-128-CCM API to provide the single
 * aes_ccm_decrypt_impl() required by ICrypto<Derived>.
 *
 * Requires: libssl-dev (apt install libssl-dev)
 */
#pragma once

#include "ICrypto.hpp"

namespace Crypto {

/**
 * @brief AES-128-CCM-4 backend using OpenSSL EVP_CIPHER_CTX.
 *
 * Stateless: every call to aes_ccm_decrypt_impl() creates and frees
 * a temporary EVP_CIPHER_CTX on the stack (via RAII wrapper), so no
 * persistent state is needed and the class is trivially default constructible.
 */
class AesCcm final : public ICrypto<AesCcm> {
   public:
    AesCcm() noexcept = default;
    ~AesCcm() noexcept = default;

    AesCcm(const AesCcm&) = delete;
    AesCcm& operator=(const AesCcm&) = delete;
    AesCcm(AesCcm&&) = delete;
    AesCcm& operator=(AesCcm&&) = delete;

    /**
     * @brief AES-128-CCM-4 tag verification (+ optional decryption).
     *
     * Verifies the CCM authentication tag and optionally decrypts ciphertext.
     * If ciphertext is empty, performs tag-only verification (authentication
     * without decryption). Uses OpenSSL EVP API for the cryptographic
     * operations.
     *
     * @param key 16-byte AES-128 key span
     * @param nonce 13-byte CCM nonce span
     * @param aad Additional authenticated data (may be empty)
     * @param ciphertext Encrypted payload to decrypt (may be empty for
     * auth-only)
     * @param tag 4-byte CCM authentication tag to verify
     * @param plaintext Output buffer for decrypted data (may be empty if no
     * ciphertext)
     * @return true if tag verification succeeds, false otherwise
     */
    [[nodiscard]] bool aes_ccm_decrypt_impl(
        std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
        std::span<const std::uint8_t> aad,
        std::span<const std::uint8_t> ciphertext,
        std::array<std::uint8_t, Enocean::k_tag_len> tag,
        std::span<std::uint8_t> plaintext) noexcept;
};

static_assert(Crypto::CryptoImplementation<AesCcm>,
              "AesCcm must satisfy Crypto::CryptoImplementation concept");

}  // namespace Crypto
