/**
 * @file MutexError.hpp
 * @brief Error handling types for mutex operations.
 *
 * This file defines error codes and traits for type-safe error handling
 * in mutex operations across all platforms (POSIX, Zephyr).
 * Uses the ErrorHandler wrapper for monadic error handling.
 */
#pragma once

#include <string_view>

#include "error_handler/ErrorHandler.hpp"

namespace PlatformCore {

/**
 * @brief Error codes for mutex operations.
 *
 * Defines all possible error conditions that can occur during mutex
 * initialization, locking, and unlocking operations. These codes are
 * platform-agnostic and map to underlying system errors.
 */
enum class MutexError {
    /// Invalid parameters or configuration
    InvalidParameter,
    /// Insufficient memory to create or initialize mutex
    InsufficientMemory,
    /// System resource limits exceeded
    ResourceLimitExceeded,
    /// Operation timed out waiting for mutex
    Timeout,
    /// Operation interrupted by signal or external event
    Interrupted,
    /// Mutex not initialized before use
    NotInitialized,
    /// Mutex already initialized
    AlreadyInitialized,
    /// Deadlock condition detected
    Deadlock,
    /// Invalid mutex handle or descriptor
    InvalidMutex,
    /// Permission denied
    PermissionDenied,
    /// Mutex is busy (locked by another thread)
    Busy,
    /// Mutex was deleted while in use
    Deleted,
    /// Wait operation was aborted
    WaitAborted,
    /// Caller error (invalid context)
    CallerError,
    /// Unknown or unclassified error
    Unknown
};

}  // namespace PlatformCore

/**
 * @brief ErrorTraits specialization for MutexError.
 *
 * Provides human-readable descriptions for each mutex error code.
 * These descriptions are used in error logging to provide clear diagnostic
 * information in safety-critical applications.
 */
template <>
struct ErrorTraits<PlatformCore::MutexError> {
    /**
     * @brief Returns a human-readable description of the error.
     * @param e The mutex error code
     * @return String view containing the error description
     */
    static constexpr std::string_view name(PlatformCore::MutexError e) {
        using PlatformCore::MutexError;
        switch (e) {
            case MutexError::InvalidParameter:
                return "Invalid Parameter";
            case MutexError::InsufficientMemory:
                return "Insufficient Memory";
            case MutexError::ResourceLimitExceeded:
                return "Resource Limit Exceeded";
            case MutexError::Timeout:
                return "Operation Timeout";
            case MutexError::Interrupted:
                return "Operation Interrupted";
            case MutexError::NotInitialized:
                return "Mutex Not Initialized";
            case MutexError::AlreadyInitialized:
                return "Mutex Already Initialized";
            case MutexError::Deadlock:
                return "Deadlock Detected";
            case MutexError::InvalidMutex:
                return "Invalid Mutex";
            case MutexError::PermissionDenied:
                return "Permission Denied";
            case MutexError::Busy:
                return "Mutex Busy";
            case MutexError::Deleted:
                return "Mutex Deleted";
            case MutexError::WaitAborted:
                return "Wait Aborted";
            case MutexError::CallerError:
                return "Caller Error";
            case MutexError::Unknown:
                return "Unknown Error";
            default:
                return "Unrecognized Mutex Error";
        }
    }
};
