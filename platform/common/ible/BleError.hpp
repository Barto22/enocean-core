/**
 * @file BleError.hpp
 * @brief BLE scanner error codes and ErrorTraits specialisation.
 */
#pragma once

#include <string_view>

#include "error_handler/ErrorHandler.hpp"

namespace Ble {

/**
 * @brief Error codes returned by the BLE scanner interface.
 */
enum class BleError : std::uint8_t {
    AlreadyInitialised,  ///< init() called more than once.
    NotInitialised,      ///< start/stop called before init().
    AdapterNotFound,     ///< No BT adapter found on the system.
    PermissionDenied,    ///< Insufficient privileges (CAP_NET_RAW etc.).
    ScanStartFailed,     ///< HCI/Zephyr scan start failed.
    ScanStopFailed,      ///< HCI/Zephyr scan stop failed.
    InvalidCallback,     ///< Null advertisement callback supplied.
    SystemError,         ///< Unclassified OS-level error.
};

}  // namespace Ble

/**
 * @brief ErrorTraits specialization for BleError.
 *
 * Provides human-readable string conversion for BleError enum values,
 * required by ErrorHandler<T, BleError> for logging.
 */
template <>
struct ErrorTraits<Ble::BleError> {
    /// @brief Returns a human-readable string for a BleError value.
    static constexpr std::string_view name(Ble::BleError e) noexcept {
        switch (e) {
            case Ble::BleError::AlreadyInitialised:
                return "AlreadyInitialised";
            case Ble::BleError::NotInitialised:
                return "NotInitialised";
            case Ble::BleError::AdapterNotFound:
                return "AdapterNotFound";
            case Ble::BleError::PermissionDenied:
                return "PermissionDenied";
            case Ble::BleError::ScanStartFailed:
                return "ScanStartFailed";
            case Ble::BleError::ScanStopFailed:
                return "ScanStopFailed";
            case Ble::BleError::InvalidCallback:
                return "InvalidCallback";
            case Ble::BleError::SystemError:
                return "SystemError";
        }
        return "Unknown";
    }
};
