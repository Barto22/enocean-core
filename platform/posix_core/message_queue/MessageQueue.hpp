/**
 * @file MessageQueue.hpp
 * @brief POSIX implementation of the message queue interface using POSIX
 * message queues.
 *
 * This file provides a concrete implementation of the IMessageQueue interface
 * for POSIX-compliant systems. It uses the POSIX message queue API (mqueue.h)
 * to provide thread-safe, inter-process message passing with support for
 * timeouts and priority.
 *
 * Key features:
 * - Named POSIX message queues for IPC (inter-process communication)
 * - Timeout support for send and receive operations
 * - FIFO message ordering with optional priority
 * - Compile-time type safety through templates
 * - Zero virtual function overhead through CRTP
 */
#pragma once

#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <time.h>

#include <cerrno>
#include <cstddef>
#include <limits>

#include "IMessageQueue.hpp"

namespace PlatformCore {

/**
 * @brief Converts errno value to MessageQueueError enum.
 *
 * Maps POSIX errno values to the platform-agnostic MessageQueueError
 * enumeration for consistent error handling across implementations.
 * Declared as a free constexpr function to ensure a single copy is shared
 * across all MessageQueue<T> template instantiations.
 *
 * @param err The errno value to convert
 * @return Corresponding MessageQueueError enum value
 */
constexpr auto errno_to_error(int err) -> MessageQueueError {
    switch (err) {
        case EINVAL:
        case EBADF:
            return MessageQueueError::InvalidParameter;
        case ENOMEM:
            return MessageQueueError::InsufficientMemory;
        case ENOSPC:
        case EMFILE:
        case ENFILE:
            return MessageQueueError::ResourceLimitExceeded;
        case ETIMEDOUT:
            return MessageQueueError::Timeout;
        case EINTR:
            return MessageQueueError::Interrupted;
        case EAGAIN:
            return MessageQueueError::QueueFull;
        case EACCES:
            return MessageQueueError::PermissionDenied;
        case EEXIST:
            return MessageQueueError::AlreadyExists;
        default:
            return MessageQueueError::Unknown;
    }
}

/**
 * @brief POSIX implementation of a type-safe message queue.
 *
 * This class implements the IMessageQueue interface using POSIX message queues
 * (mqueue.h). It provides a thread-safe, potentially inter-process message
 * passing mechanism with FIFO ordering and timeout support.
 *
 * POSIX message queues are kernel-managed objects that persist until explicitly
 * unlinked. They support both blocking and timed operations, making them
 * suitable for real-time and concurrent applications.
 *
 * @tparam T The message type to be queued (should be trivially copyable for
 * safe serialization)
 *
 * @note POSIX message queues have system-wide limits (see /proc/sys/fs/mqueue/)
 * @note The queue name must begin with '/' and follow POSIX naming conventions
 * @note Requires linking with -lrt on some systems
 *
 * Example usage:
 * @code
 * MessageQueue<MyMessage> queue("/myqueue");
 * char name[] = "/myqueue";
 * queue.init(name, 10); // Create queue with capacity of 10 messages
 * MyMessage msg{...};
 * queue.send(msg, 1000); // Send with 1 second timeout
 * @endcode
 */
template <typename T>
class MessageQueue : public IMessageQueue<MessageQueue<T>, std::nullptr_t, T,
                                          std::uint32_t, false> {
   public:
    /**
     * @brief Inherits constructors from the base IMessageQueue class.
     *
     * This using declaration brings the base class constructor into scope,
     * allowing construction with an optional name parameter.
     */
    using IMessageQueue<MessageQueue<T>, std::nullptr_t, T, std::uint32_t,
                        false>::IMessageQueue;

    /**
     * @brief Destructor that closes and unlinks the POSIX message queue.
     *
     * Automatically closes the message queue descriptor if it was successfully
     * opened and unlinks the named queue to guarantee deterministic resource
     * reclamation. The unlink step prevents stale kernel objects in long
     * running test environments and avoids leaking queue names across process
     * restarts.
     */
    ~MessageQueue();

    /**
     * @brief Initializes the POSIX message queue with specified parameters.
     *
     * Creates or opens a named POSIX message queue with the specified
     * capacity. The queue name is provided via the IMessageQueue
     * constructor and must follow POSIX naming rules (begin with '/').
     * The queue is created with read/write permissions (0644) and
     * configured to store messages of type T. If a queue with the same
     * name already exists, it will be opened.
     *
     * @param capacity Maximum number of messages the queue can hold
     * @return ErrorHandler with success (std::monostate) or MessageQueueError
     *
     * @note System limits on queue capacity may apply (check
     * /proc/sys/fs/mqueue/msg_max)
     * @note Each message can hold exactly sizeof(T) bytes
     * @note Queue attributes: non-blocking flag is disabled (mq_flags = 0)
     */
    auto init_impl(std::uint32_t capacity)
        -> ErrorHandler<std::monostate, MessageQueueError>;

    /**
     * @brief Sends a message to the queue with an optional timeout.
     *
     * Attempts to place a message into the POSIX message queue. If
     * timeout_ms is 0, uses mq_send() for an immediate blocking send.
     * Otherwise, uses mq_timedsend() with an absolute timeout
     * calculated from the current time.
     *
     * If the queue is full, the call blocks until space becomes
     * available or the timeout expires. Messages are sent with priority 0.
     *
     * @param msg The message to send (copied into the queue)
     * @param timeout_ms Timeout in milliseconds (0 or UINT32_MAX = infinite
     * wait)
     * @return ErrorHandler with success (std::monostate) or MessageQueueError
     *
     * @note The message is serialized as raw bytes (reinterpret_cast)
     * @note All messages are sent with priority 0
     */
    auto send_impl(T& msg, std::uint32_t timeout_ms) const
        -> ErrorHandler<std::monostate, MessageQueueError>;

    /**
     * @brief Receives a message from the queue with an optional timeout.
     *
     * Attempts to retrieve a message from the POSIX message queue in
     * FIFO order (or priority order if messages have different priorities).
     * If timeout_ms is 0, uses mq_receive() for immediate blocking receive.
     * Otherwise, uses mq_timedreceive() with an absolute timeout.
     *
     * If the queue is empty, the call blocks until a message arrives or
     * the timeout expires.
     *
     * @param msg Reference to store the received message
     * @param timeout_ms Timeout in milliseconds (0 or UINT32_MAX = infinite
     * wait)
     * @return ErrorHandler with success (std::monostate) or MessageQueueError
     *
     * @note The message priority is not returned (nullptr passed to priority
     * param)
     * @note The message is deserialized from raw bytes (reinterpret_cast)
     */
    auto receive_impl(T& msg, std::uint32_t timeout_ms) const
        -> ErrorHandler<std::monostate, MessageQueueError>;

    /**
     * @brief Queries the number of free message slots in the queue.
     *
     * Retrieves the current queue attributes using mq_getattr() and
     * calculates the number of available slots by subtracting the
     * current number of messages from the maximum capacity.
     *
     * @return Number of free message slots, or 0 if the query fails
     *
     * @note This is a snapshot value that may change immediately in
     * concurrent scenarios
     * @note Returns 0 on error (including invalid queue descriptor)
     */
    auto get_free_space_impl() const -> size_t;

   private:
    /**
     * @brief POSIX message queue descriptor.
     *
     * Stores the message queue descriptor returned by mq_open().
     * Initialized to -1 (invalid descriptor) until successfully opened.
     */
    mqd_t mqd_{static_cast<mqd_t>(-1)};

    /**
     * @brief Converts milliseconds to an absolute timespec for POSIX timed
     * operations.
     *
     * Calculates an absolute timespec value by adding the specified
     * timeout in milliseconds to the current real-time clock value.
     * This is required for POSIX timed operations like mq_timedsend()
     * and mq_timedreceive(), which always evaluate deadlines against
     * CLOCK_REALTIME.
     *
     * Uses CLOCK_REALTIME per POSIX mq_timedsend()/mq_timedreceive()
     * requirements. The absolute deadline is computed from the current
     * real time value so that the kernel interprets the timeout
     * correctly.
     *
     * The function properly handles nanosecond overflow by incrementing
     * seconds when nanoseconds exceed 1 second (1,000,000,000
     * nanoseconds).
     *
     * @param timeout_ms Timeout duration in milliseconds
     * @return Absolute timespec representing the timeout deadline
     *
     * @note Uses CLOCK_REALTIME because
     * mq_timedsend()/mq_timedreceive() compare deadlines against that
     * clock source
     * @note Handles nanosecond overflow correctly (when nsec >=
     * 1,000,000,000)
     */
    auto ms_to_timespec(std::uint32_t timeout_ms) const -> struct timespec;
};

}  // namespace PlatformCore

#include "MessageQueue.ipp"
