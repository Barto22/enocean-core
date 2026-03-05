/**
 * @file ICrypto.hpp
 * @brief CRTP interface for platform-specific AES-128-CCM-4 operations.
 *
 * Provides a compile-time polymorphic contract for the AES-CCM-4 decryption
 * and tag verification used by the EnOcean driver.
 *
 * The single required operation is aes_ccm_decrypt(), which:
 *   - Takes the 16-byte AES key, 13-byte nonce, additional authenticated
 *     data (AAD), optional ciphertext, and the 4-byte CCM tag.
 *   - Returns true if the tag verifies correctly.
 *   - Writes decrypted plaintext into the provided output span (which may
 *     be empty if there is no encrypted payload, as is the case for EnOcean
 *     switch advertisements where only the tag is checked).
 *
 * Usage (POSIX example):
 * @code
 *   Crypto::AesCcm crypto{};
 *   bool ok = crypto.aes_ccm_decrypt(key, nonce, aad, ciphertext, tag, plain);
 * @endcode
 */
#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <type_traits>

#include "CryptoError.hpp"
#include "enocean/EnoceanTypes.hpp"

namespace Crypto {

template <typename D>
concept CryptoImplementation = requires(
    D d, std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
    std::span<const std::uint8_t> aad, std::span<const std::uint8_t> ciphertext,
    std::array<std::uint8_t, Enocean::k_tag_len> tag,
    std::span<std::uint8_t> plaintext) {
    {
        d.aes_ccm_decrypt_impl(key, nonce, aad, ciphertext, tag, plaintext)
    } -> std::same_as<bool>;
    requires !std::is_abstract_v<D>;
};

/**
 * @brief CRTP base class for AES-128-CCM-4 platform backends.
 *
 * @tparam Derived  Concrete implementation (e.g., Crypto::AesCcm).
 */
template <typename Derived>
class ICrypto {
   public:
    ICrypto() = default;
    ICrypto(const ICrypto&) = delete;
    ICrypto& operator=(const ICrypto&) = delete;
    ICrypto(ICrypto&&) = delete;
    ICrypto& operator=(ICrypto&&) = delete;
    ~ICrypto() noexcept = default;

   public:
    /**
     * @brief AES-128-CCM-4 tag verification (and optional decryption).
     *
     * @param key        16-byte AES key.
     * @param nonce      13-byte CCM nonce.
     * @param aad        Additional authenticated data (may be empty).
     * @param ciphertext Encrypted payload (may be empty for tag-only check).
     * @param tag        4-byte CCM authentication tag to verify.
     * @param plaintext  Output buffer for decrypted bytes (may be empty).
     * @return true if the tag is valid; false otherwise.
     *
     * @note Key and nonce lengths are validated before dispatch; a length
     *       mismatch causes an immediate return of false.
     */
    [[nodiscard]] bool aes_ccm_decrypt(
        std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
        std::span<const std::uint8_t> aad,
        std::span<const std::uint8_t> ciphertext,
        std::array<std::uint8_t, Enocean::k_tag_len> tag,
        std::span<std::uint8_t> plaintext) noexcept {
        if (key.size() != Enocean::k_key_len) {
            return false;
        }
        if (nonce.size() != Enocean::k_nonce_len) {
            return false;
        }
        return static_cast<Derived*>(this)->aes_ccm_decrypt_impl(
            key, nonce, aad, ciphertext, tag, plaintext);
    }
};

}  // namespace Crypto
