/**
 * @file Crypto.hpp
 * @brief Zephyr PSA crypto implementation of ICrypto<AesCcm> for AES-128-CCM-4.
 *
 * Uses the Zephyr PSA Crypto API (psa_aead_*) which is available when
 * CONFIG_MBEDTLS or CONFIG_PSA_CRYPTO is enabled in prj.conf.
 */
#pragma once

#include "ICrypto.hpp"

namespace Crypto {

/**
 * @brief AES-128-CCM-4 backend using Zephyr PSA AEAD API.
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
     * without decryption). Uses Zephyr PSA Crypto API for the cryptographic
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
