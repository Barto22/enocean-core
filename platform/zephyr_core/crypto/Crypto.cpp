/**
 * @file Crypto.cpp
 * @brief Zephyr AES-128-CCM-4 implementation using bt_ccm_decrypt.
 *
 * AAD construction matches the NCS sdk-nrf enocean.c implementation:
 *
 *   bt_ccm_decrypt(key, nonce, tag, 0, payload, tot_len, NULL, 4)
 *
 * where payload is the raw BLE AD record from the length byte onward:
 *   [LEN][0xFF][0xDA][0x03][seq(4)][sensor_data / switch_state]
 *
 * The 'aad' parameter from EnoceanDriver contains seq+data (the bytes
 * after the 2-byte Company ID, up to but not including the 4-byte tag).
 * The LEN byte is reconstructed as:
 *   LEN = 1(TYPE=0xFF) + 2(CID) + aad.size() + ct_len + k_tag_len
 */
#include "Crypto.hpp"

#include <zephyr/bluetooth/crypto.h>

#include <algorithm>
#include <logging/Logger.hpp>

namespace Crypto {

namespace {
constexpr std::size_t k_ad_hdr{4U};
constexpr std::size_t k_max_aad{32U};
constexpr std::size_t k_max_ct{64U};
}  // namespace

bool AesCcm::aes_ccm_decrypt_impl(
    std::span<const std::uint8_t> key, std::span<const std::uint8_t> nonce,
    std::span<const std::uint8_t> aad, std::span<const std::uint8_t> ciphertext,
    std::array<std::uint8_t, Enocean::k_tag_len> tag,
    std::span<std::uint8_t> plaintext) noexcept {
    std::array<std::uint8_t, Enocean::k_nonce_len> nonce_buf{};
    std::copy(nonce.begin(), nonce.end(), nonce_buf.begin());

    const std::size_t ct_len{ciphertext.size()};

    if (aad.size() > k_max_aad) {
        LOGGER_ERROR("AesCcm: aad too large (%zu)", aad.size());
        return false;
    }

    const std::size_t len_val{1U + 2U + aad.size() + ct_len +
                              Enocean::k_tag_len};

    std::array<std::uint8_t, k_ad_hdr + k_max_aad> full_aad{};
    full_aad[0] = static_cast<std::uint8_t>(len_val);
    full_aad[1] = 0xFFU;
    full_aad[2] = 0xDAU;
    full_aad[3] = 0x03U;
    if (!aad.empty()) {
        std::copy(aad.begin(), aad.end(),
                  full_aad.begin() + static_cast<std::ptrdiff_t>(k_ad_hdr));
    }
    const std::size_t full_aad_len{k_ad_hdr + aad.size()};

    if (ct_len > k_max_ct) {
        LOGGER_ERROR("AesCcm: ciphertext too large (%zu)", ct_len);
        return false;
    }
    std::array<std::uint8_t, k_max_ct + Enocean::k_tag_len> cipher_buf{};
    if (ct_len > 0U) {
        std::copy(ciphertext.begin(), ciphertext.end(), cipher_buf.begin());
    }
    std::copy(tag.begin(), tag.end(),
              cipher_buf.begin() + static_cast<std::ptrdiff_t>(ct_len));

    const auto err{
        bt_ccm_decrypt(key.data(), nonce_buf.data(), cipher_buf.data(), ct_len,
                       full_aad.data(), full_aad_len,
                       plaintext.empty() ? nullptr : plaintext.data(),
                       static_cast<std::uint8_t>(Enocean::k_tag_len))};

    if (err != 0) {
        LOGGER_WARNING(
            "AesCcm: bt_ccm_decrypt failed: %d "
            "ct=%zu aad=%zu len=0x%02X "
            "n=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X "
            "t=%02X%02X%02X%02X "
            "k=%02X%02X%02X%02X",
            err, ct_len, aad.size(), static_cast<unsigned>(full_aad[0]),
            nonce_buf[0], nonce_buf[1], nonce_buf[2], nonce_buf[3],
            nonce_buf[4], nonce_buf[5], nonce_buf[6], nonce_buf[7],
            nonce_buf[8], nonce_buf[9], nonce_buf[10], nonce_buf[11],
            nonce_buf[12], tag[0], tag[1], tag[2], tag[3], key[0], key[1],
            key[2], key[3]);
        return false;
    }
    return true;
}

}  // namespace Crypto
