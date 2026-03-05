/**
 * @file WatchdogError.hpp
 * @brief Error codes for hardware watchdog operations
 *
 * Defines error codes that can occur during watchdog timer initialization
 * and configuration.
 */
#pragma once

#include <error_handler/ErrorHandler.hpp>

/**
 * @namespace Boards::Watchdog
 * @brief Namespace for watchdog timer abstractions and error handling
 */
namespace Boards::Watchdog {

/**
 * @enum WatchdogError
 * @brief Error codes for watchdog operations
 */
enum class WatchdogError {
    InvalidParameter,    ///< Invalid parameter provided (e.g., timeout)
    DeviceNotFound,      ///< Watchdog device not found or not available
    DeviceNotReady,      ///< Watchdog device not ready
    NotSupported,        ///< Operation not supported by hardware
    HardwareError,       ///< Hardware initialization or configuration error
    AlreadyInitialized,  ///< Watchdog already initialized
    ConfigurationError,  ///< Invalid watchdog configuration
    TimeoutOutOfRange,   ///< Requested timeout is out of valid range
    PrescalerError,      ///< Failed to find suitable prescaler value
    ReloadError,         ///< Invalid reload value
};

}  // namespace Boards::Watchdog

/**
 * @brief ErrorTraits specialization for WatchdogError
 */
template <>
struct ErrorTraits<Boards::Watchdog::WatchdogError> {
    /**
     * @brief Convert WatchdogError to human-readable string
     * @param e The error code
     * @return String view with error description
     */
    static constexpr std::string_view name(Boards::Watchdog::WatchdogError e) {
        using enum Boards::Watchdog::WatchdogError;
        switch (e) {
            case InvalidParameter:
                return "Invalid Parameter";
            case DeviceNotFound:
                return "Device Not Found";
            case DeviceNotReady:
                return "Device Not Ready";
            case NotSupported:
                return "Not Supported";
            case HardwareError:
                return "Hardware Error";
            case AlreadyInitialized:
                return "Already Initialized";
            case ConfigurationError:
                return "Configuration Error";
            case TimeoutOutOfRange:
                return "Timeout Out Of Range";
            case PrescalerError:
                return "Prescaler Error";
            case ReloadError:
                return "Reload Error";
        }
        return "Unknown Watchdog Error";
    }
};
