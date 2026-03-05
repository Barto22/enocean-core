/**
 * @file CryptoError.hpp
 * @brief Crypto backend error codes and ErrorTraits specialisation.
 */
#pragma once

#include <string_view>

#include "error_handler/ErrorHandler.hpp"

namespace Crypto {

/**
 * @brief Error codes returned by the AES-CCM crypto interface.
 */
enum class CryptoError : std::uint8_t {
    NotInitialised,  ///< Backend not initialised.
    InvalidKey,      ///< Key length is not 16 bytes.
    InvalidNonce,    ///< Nonce length is not 13 bytes.
    InvalidTag,      ///< Tag length is not 4 bytes.
    AuthFailed,      ///< AES-CCM tag verification failed.
    SystemError,     ///< Unclassified OS/library error.
};

}  // namespace Crypto

/**
 * @brief ErrorTraits specialization for CryptoError.
 *
 * Provides human-readable string conversion for CryptoError enum values,
 * required by ErrorHandler<T, CryptoError> for logging.
 */
template <>
struct ErrorTraits<Crypto::CryptoError> {
    /// @brief Returns a human-readable string for a CryptoError value.
    static constexpr std::string_view name(Crypto::CryptoError e) noexcept {
        switch (e) {
            case Crypto::CryptoError::NotInitialised:
                return "NotInitialised";
            case Crypto::CryptoError::InvalidKey:
                return "InvalidKey";
            case Crypto::CryptoError::InvalidNonce:
                return "InvalidNonce";
            case Crypto::CryptoError::InvalidTag:
                return "InvalidTag";
            case Crypto::CryptoError::AuthFailed:
                return "AuthFailed";
            case Crypto::CryptoError::SystemError:
                return "SystemError";
        }
        return "Unknown";
    }
};
