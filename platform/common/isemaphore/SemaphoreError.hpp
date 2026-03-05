/**
 * @file SemaphoreError.hpp
 * @brief Error handling types for semaphore operations.
 *
 * This file defines error codes and traits for type-safe error handling
 * in semaphore operations across all platforms (POSIX, Zephyr).
 * Uses the ErrorHandler wrapper for monadic error handling.
 */
#pragma once

#include <string_view>

#include "error_handler/ErrorHandler.hpp"

namespace PlatformCore {

/**
 * @brief Error codes for semaphore operations.
 *
 * Defines all possible error conditions that can occur during semaphore
 * initialization, acquisition (take), and release (give) operations.
 * These codes are platform-agnostic and map to underlying system errors.
 */
enum class SemaphoreError {
    /// Invalid parameters or configuration
    InvalidParameter,
    /// Insufficient memory to create or initialize semaphore
    InsufficientMemory,
    /// System resource limits exceeded
    ResourceLimitExceeded,
    /// Operation timed out waiting for semaphore
    Timeout,
    /// Operation interrupted by signal or external event
    Interrupted,
    /// Semaphore not initialized before use
    NotInitialized,
    /// Semaphore already initialized
    AlreadyInitialized,
    /// Invalid semaphore handle or descriptor
    InvalidSemaphore,
    /// Permission denied
    PermissionDenied,
    /// Semaphore count would overflow
    CountOverflow,
    /// Semaphore was deleted while in use
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
 * @brief ErrorTraits specialization for SemaphoreError.
 *
 * Provides human-readable descriptions for each semaphore error code.
 * These descriptions are used in error logging to provide clear diagnostic
 * information in safety-critical applications.
 */
template <>
struct ErrorTraits<PlatformCore::SemaphoreError> {
    /**
     * @brief Returns a human-readable description of the error.
     * @param e The semaphore error code
     * @return String view containing the error description
     */
    static constexpr std::string_view name(PlatformCore::SemaphoreError e) {
        using PlatformCore::SemaphoreError;
        switch (e) {
            case SemaphoreError::InvalidParameter:
                return "Invalid Parameter";
            case SemaphoreError::InsufficientMemory:
                return "Insufficient Memory";
            case SemaphoreError::ResourceLimitExceeded:
                return "Resource Limit Exceeded";
            case SemaphoreError::Timeout:
                return "Operation Timeout";
            case SemaphoreError::Interrupted:
                return "Operation Interrupted";
            case SemaphoreError::NotInitialized:
                return "Semaphore Not Initialized";
            case SemaphoreError::AlreadyInitialized:
                return "Semaphore Already Initialized";
            case SemaphoreError::InvalidSemaphore:
                return "Invalid Semaphore";
            case SemaphoreError::PermissionDenied:
                return "Permission Denied";
            case SemaphoreError::CountOverflow:
                return "Count Overflow";
            case SemaphoreError::Deleted:
                return "Semaphore Deleted";
            case SemaphoreError::WaitAborted:
                return "Wait Aborted";
            case SemaphoreError::CallerError:
                return "Caller Error";
            case SemaphoreError::Unknown:
                return "Unknown Error";
            default:
                return "Unrecognized Semaphore Error";
        }
    }
};
