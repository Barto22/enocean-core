/**
 * @file MessageQueueError.hpp
 * @brief Error handling types for message queue operations.
 *
 * This file defines error codes and traits for type-safe error handling
 * in message queue operations across all platforms (POSIX, Zephyr).
 * Uses the ErrorHandler wrapper for monadic error handling.
 */
#pragma once

#include <string_view>

#include "error_handler/ErrorHandler.hpp"

namespace PlatformCore {

/**
 * @brief Error codes for message queue operations.
 *
 * Defines all possible error conditions that can occur during message queue
 * initialization, sending, and receiving operations. These codes are
 * platform-agnostic and map to underlying system errors.
 */
enum class MessageQueueError {
    /// Invalid parameters or configuration
    InvalidParameter,
    /// Insufficient memory to create or initialize queue
    InsufficientMemory,
    /// System resource limits exceeded
    ResourceLimitExceeded,
    /// Operation timed out waiting for space/message
    Timeout,
    /// Operation interrupted by signal or external event
    Interrupted,
    /// Queue full, cannot send message
    QueueFull,
    /// Queue empty, cannot receive message
    QueueEmpty,
    /// Invalid queue handle or descriptor
    InvalidQueue,
    /// Queue already initialized
    AlreadyInitialized,
    /// Permission denied accessing queue
    PermissionDenied,
    /// Queue name already exists
    AlreadyExists,
    /// Unknown or unclassified error
    Unknown
};

}  // namespace PlatformCore

/**
 * @brief ErrorTraits specialization for MessageQueueError.
 *
 * Provides human-readable descriptions for each message queue error code.
 * These descriptions are used in error logging to provide clear diagnostic
 * information in safety-critical applications.
 */
template <>
struct ErrorTraits<PlatformCore::MessageQueueError> {
    /**
     * @brief Returns a human-readable description of the error.
     * @param e The message queue error code
     * @return String view containing the error description
     */
    static constexpr std::string_view name(PlatformCore::MessageQueueError e) {
        using PlatformCore::MessageQueueError;
        switch (e) {
            case MessageQueueError::InvalidParameter:
                return "Invalid Parameter";
            case MessageQueueError::InsufficientMemory:
                return "Insufficient Memory";
            case MessageQueueError::ResourceLimitExceeded:
                return "Resource Limit Exceeded";
            case MessageQueueError::Timeout:
                return "Operation Timeout";
            case MessageQueueError::Interrupted:
                return "Operation Interrupted";
            case MessageQueueError::QueueFull:
                return "Queue Full";
            case MessageQueueError::QueueEmpty:
                return "Queue Empty";
            case MessageQueueError::InvalidQueue:
                return "Invalid Queue";
            case MessageQueueError::AlreadyInitialized:
                return "Already Initialized";
            case MessageQueueError::PermissionDenied:
                return "Permission Denied";
            case MessageQueueError::AlreadyExists:
                return "Already Exists";
            case MessageQueueError::Unknown:
                return "Unknown Error";
            default:
                return "Unrecognized MessageQueue Error";
        }
    }
};
