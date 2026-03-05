/**
 * @file EnoceanError.hpp
 * @brief EnOcean driver error codes and ErrorTraits specialisation.
 *
 * Defines the EnoceanError enumeration used throughout the EnOcean driver
 * and provides the ErrorTraits specialisation required by ErrorHandler<T,E>.
 */
#pragma once

#include <string_view>

#include "error_handler/ErrorHandler.hpp"

namespace Enocean {

/**
 * @brief Error codes returned by the EnOcean driver.
 */
enum class EnoceanError : std::uint8_t {
    InvalidParameter,        ///< Null or out-of-range argument
    NotInitialised,          ///< Driver not yet initialised
    DeviceTableFull,         ///< Maximum commissioned devices reached
    DeviceNotFound,          ///< Address not in commission table
    AuthenticationFailed,    ///< AES-CCM tag mismatch
    ReplayDetected,          ///< Sequence number not greater than stored value
    MalformedAdvertisement,  ///< AD structure shorter than expected
    CryptoError,             ///< Underlying crypto operation failed
    Unknown,                 ///< Unclassified error
};

}  // namespace Enocean

/**
 * @brief ErrorTraits specialisation for EnoceanError.
 *
 * Required by ErrorHandler<T, EnoceanError> to convert enum values to
 * human-readable strings for logging.
 */
template <>
struct ErrorTraits<Enocean::EnoceanError> {
    /// @brief Returns a human-readable string for an EnoceanError value.
    static constexpr std::string_view name(Enocean::EnoceanError e) noexcept {
        switch (e) {
            case Enocean::EnoceanError::InvalidParameter:
                return "InvalidParameter";
            case Enocean::EnoceanError::NotInitialised:
                return "NotInitialised";
            case Enocean::EnoceanError::DeviceTableFull:
                return "DeviceTableFull";
            case Enocean::EnoceanError::DeviceNotFound:
                return "DeviceNotFound";
            case Enocean::EnoceanError::AuthenticationFailed:
                return "AuthenticationFailed";
            case Enocean::EnoceanError::ReplayDetected:
                return "ReplayDetected";
            case Enocean::EnoceanError::MalformedAdvertisement:
                return "MalformedAdvertisement";
            case Enocean::EnoceanError::CryptoError:
                return "CryptoError";
            case Enocean::EnoceanError::Unknown:
                return "Unknown";
        }
        return "Unknown";
    }
};
