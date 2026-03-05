/**
 * @file MessageQueue.hpp
 * @brief Zephyr RTOS implementation of the message queue interface using Zephyr
 * message queues.
 *
 * This file provides a concrete implementation of the IMessageQueue interface
 * for Zephyr RTOS. It uses the Zephyr kernel message queue API (k_msgq) to
 * provide thread-safe message passing with support for timeouts and FIFO
 * ordering.
 *
 * Key features:
 * - Native Zephyr message queue objects for efficient intra-RTOS communication
 * - Timeout support using k_timeout_t (K_NO_WAIT, K_FOREVER, or K_MSEC/K_TICKS)
 * - FIFO message ordering
 * - Compile-time type safety through templates
 * - Zero virtual function overhead through CRTP
 * - User-provided buffer for queue storage
 * - Static assertion to verify interface compliance
 */
#pragma once

#include <zephyr/kernel.h>

#include <cerrno>

#include "IMessageQueue.hpp"
#include "MessageQueueError.hpp"

namespace PlatformCore {

/**
 * @brief Maps Zephyr error codes to MessageQueueError enum values.
 *
 * Converts Zephyr kernel error codes (-EINVAL, -EAGAIN, etc.) to the
 * platform-agnostic MessageQueueError enumeration for consistent error
 * handling.
 *
 * @param error Zephyr error code (negative errno values)
 * @return Corresponding MessageQueueError enum value
 */
constexpr auto k_error_to_error(int error) -> MessageQueueError {
    switch (error) {
        case -EINVAL:
            return MessageQueueError::InvalidParameter;
        case -EALREADY:
            return MessageQueueError::AlreadyInitialized;
        case -EAGAIN:
        case -EBUSY:
        case -ETIMEDOUT:
            return MessageQueueError::Timeout;
        case -ENOMEM:
            return MessageQueueError::InsufficientMemory;
        default:
            return MessageQueueError::Unknown;
    }
}

/**
 * @brief Zephyr RTOS implementation of a type-safe message queue.
 *
 * This class implements the IMessageQueue interface using Zephyr kernel message
 * queues (k_msgq). It provides a thread-safe message passing mechanism with
 * FIFO ordering and timeout support. Zephyr message queues are lightweight
 * kernel objects designed for efficient inter-thread communication.
 *
 * Zephyr message queues store fixed-size messages in a user-provided memory
 * buffer. Each message is copied into and out of the queue, making them
 * suitable for passing data between threads without pointer sharing. The queue
 * supports blocking, non-blocking, and timed operations.
 *
 * @tparam T The message type to be queued (should be trivially copyable for
 * safe byte-wise copying)
 *
 * @note Zephyr message queues require a buffer sized as: capacity * sizeof(T)
 * @note Timeout values use k_timeout_t type (K_NO_WAIT, K_FOREVER, K_MSEC(),
 * etc.)
 * @note The buffer must be properly aligned for the message type
 * @note Static assertion ensures compile-time interface compliance
 *
 * Example usage:
 * @code
 * MessageQueue<MyMessage> queue("myqueue");
 * char buffer[10 * sizeof(MyMessage)];
 * queue.init(buffer, 10);  // 10 message capacity
 * MyMessage msg{...};
 * queue.send(msg, K_FOREVER);  // Send with infinite timeout
 * @endcode
 */
template <typename T>
class MessageQueue
    : public IMessageQueue<MessageQueue<T>, char, T, k_timeout_t> {
   public:
    /**
     * @brief Inherits constructors from the base IMessageQueue class.
     *
     * This using declaration brings the base class constructor into scope,
     * allowing construction with an optional name parameter for debugging and
     * tracing.
     */
    using IMessageQueue<MessageQueue<T>, char, T, k_timeout_t>::IMessageQueue;

    /**
     * @brief Initializes the Zephyr message queue with specified parameters.
     *
     * Initializes a Zephyr kernel message queue using k_msgq_init(). The
     * function configures the queue to store messages of type T with the
     * specified capacity. The buffer must be large enough to hold capacity *
     * sizeof(T) bytes.
     *
     * @param buffer Pointer to the memory buffer for storing queue messages.
     *               Must be non-null and aligned for `T`.
     * @param capacity Number of messages the queue can hold (not buffer size in
     *                 bytes). Must be greater than zero.
     * @return ErrorHandler with success or MessageQueueError on failure
     *
     * @note Buffer must be at least capacity * sizeof(T) bytes
     * @note Buffer should be properly aligned for type T
     * @note The buffer must persist for the lifetime of the queue
     */
    auto init_impl(char* buffer, std::uint32_t capacity)
        -> ErrorHandler<std::monostate, MessageQueueError>;

    /**
     * @brief Sends a message to the queue with a specified timeout.
     *
     * Attempts to place a message into the Zephyr message queue. If the queue
     * is full, the calling thread is blocked until space becomes available or
     * the timeout expires. The message is copied byte-wise into the queue
     * storage.
     *
     * The timeout parameter supports various Zephyr timeout constructs:
     * - K_NO_WAIT: Return immediately if queue is full (non-blocking)
     * - K_FOREVER: Wait indefinitely until space is available
     * - K_MSEC(ms): Wait up to specified milliseconds
     * - K_TICKS(ticks): Wait up to specified system ticks
     *
     * @param msg The message to send (copied into the queue)
     * @param timeout Timeout using k_timeout_t (K_NO_WAIT, K_FOREVER, K_MSEC(),
     *        etc.)
     * @return ErrorHandler with success or MessageQueueError on failure
     *
     * @note Messages are delivered in strict FIFO order
     * @note Can be called from ISR context only with K_NO_WAIT timeout
     */
    auto send_impl(T& msg, k_timeout_t timeout) const
        -> ErrorHandler<std::monostate, MessageQueueError>;

    /**
     * @brief Receives a message from the queue with a specified timeout.
     *
     * Attempts to retrieve a message from the Zephyr message queue in FIFO
     * order. If the queue is empty, the calling thread is blocked until a
     * message arrives or the timeout expires. The message is copied byte-wise
     * from the queue storage into the provided reference.
     *
     * The timeout parameter supports various Zephyr timeout constructs:
     * - K_NO_WAIT: Return immediately if queue is empty (non-blocking)
     * - K_FOREVER: Wait indefinitely until a message is available
     * - K_MSEC(ms): Wait up to specified milliseconds
     * - K_TICKS(ticks): Wait up to specified system ticks
     *
     * @param msg Reference to store the received message
     * @param timeout Timeout using k_timeout_t (K_NO_WAIT, K_FOREVER, K_MSEC(),
     *        etc.)
     * @return ErrorHandler with success or MessageQueueError on failure
     *
     * @note Messages are retrieved in strict FIFO order
     * @note Can be called from ISR context only with K_NO_WAIT timeout
     */
    auto receive_impl(T& msg, k_timeout_t timeout) const
        -> ErrorHandler<std::monostate, MessageQueueError>;

    /**
     * @brief Queries the number of free message slots available in the queue.
     *
     * Returns the current number of available message slots in the queue using
     * the Zephyr kernel function k_msgq_num_free_get(). This indicates how many
     * additional messages can be sent before the queue becomes full.
     *
     * @return Number of free message slots currently available
     *
     * @note This is a snapshot value that may change immediately in concurrent
     * scenarios
     * @note The function is thread-safe and can be called from any context
     */
    auto get_free_space_impl() const -> size_t;

   private:
    /**
     * @brief Zephyr message queue structure.
     *
     * Contains all the state and metadata for the Zephyr message queue object.
     * This structure is initialized by k_msgq_init() and used by all k_msgq_*
     * functions.
     */
    mutable k_msgq queue_{};
    bool initialized_{false};
};

}  // namespace PlatformCore

#include "MessageQueue.ipp"
