/**
 * @file GpioError.hpp
 * @brief Error handling types for GPIO operations.
 *
 * This file defines error codes and traits for type-safe error handling
 * in GPIO operations across all platforms (POSIX, Zephyr).
 * Uses the ErrorHandler wrapper for monadic error handling.
 */
#pragma once

#include <string_view>

#include "error_handler/ErrorHandler.hpp"

namespace Boards::Gpio {

/**
 * @brief Error codes for GPIO operations.
 *
 * Defines all possible error conditions that can occur during GPIO
 * pin configuration, reading, writing, and management operations.
 * These codes are platform-agnostic and map to underlying system errors.
 */
enum class GpioError {
    /// Invalid parameters or configuration
    InvalidParameter,
    /// Invalid pin identifier or out of range
    InvalidPin,
    /// Invalid GPIO mode
    InvalidMode,
    /// Hardware I/O error
    IOError,
    /// Device not available or not initialized
    DeviceNotAvailable,
    /// Insufficient memory for operation
    InsufficientMemory,
    /// Operation timeout
    Timeout,
    /// Pin already configured or in use
    PinInUse,
    /// Permission denied
    PermissionDenied,
    /// Resource busy
    Busy,
    /// Operation not supported
    NotSupported,
    /// Hardware fault detected
    HardwareFault,
    /// Unknown error
    Unknown
};

}  // namespace Boards::Gpio

/**
 * @brief ErrorTraits specialization for GpioError.
 *
 * Provides human-readable error messages for GPIO error codes.
 * Used by ErrorHandler for automatic error logging.
 */
template <>
struct ErrorTraits<Boards::Gpio::GpioError> {
    /**
     * @brief Returns human-readable name for GPIO error.
     * @param e The GpioError value
     * @return String view containing the error description
     */
    static constexpr std::string_view name(Boards::Gpio::GpioError e) {
        using Boards::Gpio::GpioError;
        switch (e) {
            case GpioError::InvalidParameter:
                return "Invalid Parameter";
            case GpioError::InvalidPin:
                return "Invalid Pin";
            case GpioError::InvalidMode:
                return "Invalid Mode";
            case GpioError::IOError:
                return "I/O Error";
            case GpioError::DeviceNotAvailable:
                return "Device Not Available";
            case GpioError::InsufficientMemory:
                return "Insufficient Memory";
            case GpioError::Timeout:
                return "Timeout";
            case GpioError::PinInUse:
                return "Pin In Use";
            case GpioError::PermissionDenied:
                return "Permission Denied";
            case GpioError::Busy:
                return "Busy";
            case GpioError::NotSupported:
                return "Not Supported";
            case GpioError::HardwareFault:
                return "Hardware Fault";
            case GpioError::Unknown:
                return "Unknown Error";
        }
        return "Unknown GPIO Error";
    }
};
