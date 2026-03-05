/**
 * @file Crypto.cpp
 * @brief OpenSSL AES-128-CCM-4 implementation.
 *
 * Uses EVP_CIPHER_CTX with EVP_aes_128_ccm() cipher.
 * The CCM tag length is fixed at Enocean::k_tag_len (4 bytes).
 *
 * OpenSSL CCM decryption procedure:
 *   1. Create context and set cipher.
 *   2. Set nonce length (EVP_CTRL_CCM_SET_IVLEN).
 *   3. Set tag and tag length (EVP_CTRL_CCM_SET_TAG).
 *   4. Provide key and nonce via DecryptInit_ex.
 *   5. Tell OpenSSL the total ciphertext length (DecryptUpdate with null out).
 *   6. Provide AAD if any (DecryptUpdate with null out and aad).
 *   7. Call DecryptUpdate with output buffer and ciphertext (or 0-length for
 *      auth-only). In CCM mode this call performs tag verification and returns
 *      a negative value on failure. EVP_DecryptFinal_ex is NOT used because in
 *      CCM mode (unlike GCM) tag verification happens inside DecryptUpdate,
 *      not in Final_ex.
 */
#include "Crypto.hpp"

#include <openssl/evp.h>

#include <logging/Logger.hpp>

namespace Crypto {

namespace {

/**
 * @brief RAII wrapper around EVP_CIPHER_CTX to guarantee ctx is freed.
 */
class EvpCtxGuard {
   public:
    explicit EvpCtxGuard() noexcept : ctx_{EVP_CIPHER_CTX_new()} {}
    ~EvpCtxGuard() noexcept {
        if (ctx_ != nullptr) {
            EVP_CIPHER_CTX_free(ctx_);
        }
    }

    EvpCtxGuard(const EvpCtxGuard&) = delete;
    EvpCtxGuard& operator=(const EvpCtxGuard&) = delete;
    EvpCtxGuard(EvpCtxGuard&&) = delete;
    EvpCtxGuard& operator=(EvpCtxGuard&&) = delete;

    [[nodiscard]] EVP_CIPHER_CTX* get() const noexcept { return ctx_; }
    [[nodiscard]] bool valid() const noexcept { return ctx_ != nullptr; }

   private:
    EVP_CIPHER_CTX* ctx_;
};

}  // namespace

bool AesCcm::aes_ccm_decrypt_impl(
    std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
    std::span<const std::uint8_t> aad, std::span<const std::uint8_t> ciphertext,
    std::array<std::uint8_t, Enocean::k_tag_len> tag,
    std::span<std::uint8_t> plaintext) noexcept {
    EvpCtxGuard guard{};
    if (!guard.valid()) {
        LOGGER_ERROR("AesCcm: EVP_CIPHER_CTX_new failed");
        return false;
    }
    EVP_CIPHER_CTX* ctx{guard.get()};

    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_ccm(), nullptr, nullptr, nullptr) !=
        1) {
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_IVLEN,
                            static_cast<int>(nonce.size()), nullptr) != 1) {
        return false;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_TAG,
                            static_cast<int>(Enocean::k_tag_len),
                            static_cast<void*>(tag.data())) != 1) {
        return false;
    }

    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) !=
        1) {
        return false;
    }

    int outl{0};

    if (EVP_DecryptUpdate(ctx, nullptr, &outl, nullptr,
                          static_cast<int>(ciphertext.size())) != 1) {
        return false;
    }

    if (!aad.empty()) {
        if (EVP_DecryptUpdate(ctx, nullptr, &outl, aad.data(),
                              static_cast<int>(aad.size())) != 1) {
            return false;
        }
    }

    // For CCM, tag verification happens inside EVP_DecryptUpdate (not
    // EVP_DecryptFinal_ex). A non-null output pointer is required even for
    // auth-only (empty ciphertext) to trigger the internal CBC-MAC check.
    // EVP_DecryptFinal_ex is intentionally not called.
    std::array<std::uint8_t, 1U> dummy_out{};
    std::uint8_t* const out_ptr{ciphertext.empty() ? dummy_out.data()
                                                   : plaintext.data()};

    if (!ciphertext.empty() && (plaintext.size() < ciphertext.size())) {
        LOGGER_ERROR("AesCcm: plaintext buffer too small");
        return false;
    }

    const int ret{EVP_DecryptUpdate(
        ctx, out_ptr, &outl,
        ciphertext.empty() ? nullptr : ciphertext.data(),
        static_cast<int>(ciphertext.size()))};
    return ret >= 0;
}

}  // namespace Crypto
