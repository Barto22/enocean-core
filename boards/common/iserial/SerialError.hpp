/**
 * @file SerialError.hpp
 * @brief Error codes for serial/UART operations
 *
 * Defines error codes that can occur during serial communication operations.
 */
#pragma once

#include <error_handler/ErrorHandler.hpp>

/**
 * @namespace Boards::Serial
 * @brief Namespace for serial/UART abstractions and error handling
 */
namespace Boards::Serial {

/**
 * @enum SerialError
 * @brief Error codes for serial operations
 */
enum class SerialError {
    InvalidParameter,      ///< Invalid parameter provided
    InsufficientMemory,    ///< Not enough memory for operation
    DeviceNotFound,        ///< Serial device not found or not available
    IOError,               ///< Hardware I/O error
    InvalidConfiguration,  ///< Invalid serial configuration
    Timeout,               ///< Operation timed out
    DeviceBusy,            ///< Device is busy
    NotInitialized,        ///< Device not initialized
    AlreadyInitialized,    ///< Device already initialized
    InitializationFailed,  ///< Initialization failed
    TransmitError,         ///< Error during transmission
    ReceiveError,          ///< Error during reception
    OverrunError,          ///< Data overrun error
    FramingError,          ///< Framing error
    ParityError,           ///< Parity error
    NoiseError,            ///< Noise detected on line
    BreakCondition,        ///< Break condition detected
    DmaError,              ///< DMA transfer error
    HardwareFault,         ///< Hardware fault detected
};

}  // namespace Boards::Serial

/**
 * @brief ErrorTraits specialization for SerialError
 */
template <>
struct ErrorTraits<Boards::Serial::SerialError> {
    /**
     * @brief Convert SerialError to human-readable string
     * @param e The error code
     * @return String view with error description
     */
    static constexpr std::string_view name(Boards::Serial::SerialError e) {
        using enum Boards::Serial::SerialError;
        switch (e) {
            case InvalidParameter:
                return "Invalid Parameter";
            case InsufficientMemory:
                return "Insufficient Memory";
            case DeviceNotFound:
                return "Device Not Found";
            case IOError:
                return "I/O Error";
            case InvalidConfiguration:
                return "Invalid Configuration";
            case Timeout:
                return "Timeout";
            case DeviceBusy:
                return "Device Busy";
            case NotInitialized:
                return "Not Initialized";
            case AlreadyInitialized:
                return "Already Initialized";
            case InitializationFailed:
                return "Initialization Failed";
            case TransmitError:
                return "Transmit Error";
            case ReceiveError:
                return "Receive Error";
            case OverrunError:
                return "Overrun Error";
            case FramingError:
                return "Framing Error";
            case ParityError:
                return "Parity Error";
            case NoiseError:
                return "Noise Error";
            case BreakCondition:
                return "Break Condition";
            case DmaError:
                return "DMA Error";
            case HardwareFault:
                return "Hardware Fault";
        }
        return "Unknown Serial Error";
    }
};
