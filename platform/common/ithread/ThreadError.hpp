/**
 * @file ThreadError.hpp
 * @brief Error handling types for thread operations.
 *
 * This file defines error codes and traits for type-safe error handling
 * in thread operations across all platforms (POSIX, Zephyr).
 * Uses the ErrorHandler wrapper for monadic error handling.
 */
#pragma once

#include <string_view>

#include "error_handler/ErrorHandler.hpp"

namespace PlatformCore {

/**
 * @brief Error codes for thread operations.
 *
 * Defines all possible error conditions that can occur during thread
 * creation, destruction, and management operations. These codes are
 * platform-agnostic and map to underlying system errors.
 */
enum class ThreadError {
    /// Invalid parameters or configuration
    InvalidParameter,
    /// Insufficient memory to create or initialize thread
    InsufficientMemory,
    /// System resource limits exceeded
    ResourceLimitExceeded,
    /// Permission denied or insufficient privileges
    PermissionDenied,
    /// Thread not found or does not exist
    ThreadNotFound,
    /// Thread already created or active
    AlreadyCreated,
    /// Deadlock condition detected
    Deadlock,
    /// Invalid thread handle or descriptor
    InvalidThread,
    /// Thread was deleted while in use
    Deleted,
    /// Caller error (invalid context)
    CallerError,
    /// Invalid stack size or configuration
    InvalidStack,
    /// Invalid priority value
    InvalidPriority,
    /// Thread creation failed
    CreationFailed,
    /// Unknown or unclassified error
    Unknown
};

}  // namespace PlatformCore

/**
 * @brief ErrorTraits specialization for ThreadError.
 *
 * Provides human-readable descriptions for each thread error code.
 * These descriptions are used in error logging to provide clear diagnostic
 * information in safety-critical applications.
 */
template <>
struct ErrorTraits<PlatformCore::ThreadError> {
    /**
     * @brief Returns a human-readable description of the error.
     * @param e The thread error code
     * @return String view containing the error description
     */
    static constexpr std::string_view name(PlatformCore::ThreadError e) {
        using PlatformCore::ThreadError;
        switch (e) {
            case ThreadError::InvalidParameter:
                return "Invalid Parameter";
            case ThreadError::InsufficientMemory:
                return "Insufficient Memory";
            case ThreadError::ResourceLimitExceeded:
                return "Resource Limit Exceeded";
            case ThreadError::PermissionDenied:
                return "Permission Denied";
            case ThreadError::ThreadNotFound:
                return "Thread Not Found";
            case ThreadError::AlreadyCreated:
                return "Thread Already Created";
            case ThreadError::Deadlock:
                return "Deadlock Detected";
            case ThreadError::InvalidThread:
                return "Invalid Thread";
            case ThreadError::Deleted:
                return "Thread Deleted";
            case ThreadError::CallerError:
                return "Caller Error";
            case ThreadError::InvalidStack:
                return "Invalid Stack";
            case ThreadError::InvalidPriority:
                return "Invalid Priority";
            case ThreadError::CreationFailed:
                return "Thread Creation Failed";
            case ThreadError::Unknown:
                return "Unknown Error";
            default:
                return "Unrecognized Thread Error";
        }
    }
};
