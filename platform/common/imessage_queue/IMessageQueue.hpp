/**
 * @file IMessageQueue.hpp
 * @brief CRTP-based message queue interface for platform-agnostic message
 * passing.
 *
 * This file defines a Curiously Recurring Template Pattern (CRTP) interface for
 * implementing thread-safe message queues across different platforms (POSIX,
 * Zephyr). The interface provides a unified API for initialization,
 * sending, receiving, and querying message queue state.
 *
 * The design uses compile-time polymorphism via CRTP to eliminate virtual
 * function overhead while ensuring type safety through C++20 concepts.
 */
#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gsl/gsl>
#include <string_view>
#include <type_traits>
#include <variant>

#include "MessageQueueError.hpp"

namespace PlatformCore {

/**
 * @brief Concept to validate message types are suitable for queue storage.
 *
 * Ensures the message type is trivially copyable and properly aligned for
 * the underlying queue implementation.
 *
 * @tparam T The message type to validate
 */
template <typename T>
concept MessageQueueCompatible =
    std::is_trivially_copyable_v<T> && (sizeof(T) > 0) &&
    (alignof(T) <= alignof(::max_align_t));

/**
 * @brief Concept that defines the required interface for message queue
 * implementations.
 *
 * This concept ensures that derived classes implement all necessary methods for
 * message queue operations. It validates at compile-time that the
 * implementation provides:
 * - Initialization with a buffer and capacity
 * - Sending messages with timeout support
 * - Receiving messages with timeout support
 * - Querying available space in the queue
 *
 * @tparam D The derived implementation class
 * @tparam BufferType The type of the underlying queue buffer
 * @tparam T The message type to be stored in the queue
 * @tparam DurationType The type used for timeout durations (e.g.,
 * std::chrono::milliseconds)
 */
template <typename D, typename BufferType>
concept QueueInitWithBuffer =
    requires(D d, gsl::not_null<BufferType*> buffer, const std::uint32_t val) {
        {
            d.init_impl(buffer, val)
        } -> std::same_as<ErrorHandler<std::monostate, MessageQueueError>>;
    };

template <typename D>
concept QueueInitWithoutBuffer = requires(D d, const std::uint32_t val) {
    {
        d.init_impl(val)
    } -> std::same_as<ErrorHandler<std::monostate, MessageQueueError>>;
};

template <bool RequiresBuffer, typename D, typename BufferType>
struct QueueInitHelper;

/**
 * @brief Helper that validates message-queue implementations requiring an
 *        external storage buffer.
 *
 * When @p RequiresBuffer is true, this specialization checks that the derived
 * type exposes an `init_impl(BufferType*, uint32_t)` signature returning
 * an ErrorHandler via the QueueInitWithBuffer concept.
 */
template <typename D, typename BufferType>
struct QueueInitHelper<true, D, BufferType> {
    /** @brief True when the derived queue satisfies QueueInitWithBuffer. */
    static constexpr bool value = QueueInitWithBuffer<D, BufferType>;
};

/**
 * @brief Helper that validates message-queue implementations managing their
 *        own storage internally.
 *
 * When @p RequiresBuffer is false, this specialization verifies that the
 * derived type provides an `init_impl(uint32_t)` function returning
 * an ErrorHandler using the QueueInitWithoutBuffer concept.
 */
template <typename D, typename BufferType>
struct QueueInitHelper<false, D, BufferType> {
    /** @brief True when the derived queue satisfies QueueInitWithoutBuffer. */
    static constexpr bool value = QueueInitWithoutBuffer<D>;
};

template <typename D, typename BufferType, typename T, typename DurationType,
          bool RequiresBuffer>
concept MessageQueueImplementation =
    QueueInitHelper<RequiresBuffer, D, BufferType>::value &&
    requires(D d, T& msg, T& const_msg, DurationType timeout) {
        {
            d.send_impl(const_msg, timeout)
        } -> std::same_as<ErrorHandler<std::monostate, MessageQueueError>>;
        {
            d.receive_impl(msg, timeout)
        } -> std::same_as<ErrorHandler<std::monostate, MessageQueueError>>;
        { d.get_free_space_impl() } -> std::same_as<size_t>;
    };

/**
 * @brief CRTP base class for platform-specific message queue implementations.
 *
 * This class provides a common interface for message queues using the Curiously
 * Recurring Template Pattern (CRTP). It delegates actual implementation to the
 * derived class while providing a consistent API and handling common
 * functionality like name management.
 *
 * Message queues enable thread-safe communication between tasks by providing
 * FIFO (First-In-First-Out) message passing with blocking and timeout
 * capabilities.
 *
 * @tparam Derived The platform-specific derived class (e.g., PosixMessageQueue)
 * @tparam BufferType The type of the underlying storage buffer
 * @tparam T The message type to be queued (must be trivially copyable)
 * @tparam DurationType Duration type for timeout operations (e.g.,
 * std::chrono::milliseconds)
 * @tparam RequiresBuffer Set to false if the derived queue does not require a
 * caller-provided buffer during initialization (e.g., POSIX mqueue)
 *
 * @note The derived class must satisfy the MessageQueueImplementation concept.
 * @note Message queues are non-copyable and non-movable to prevent resource
 * management issues.
 */
template <typename Derived, typename BufferType, typename T,
          typename DurationType, bool RequiresBuffer = true>
    requires MessageQueueCompatible<T>
class IMessageQueue {
   public:
    /**
     * @brief Constructs a message queue with an optional name.
     *
     * Initializes the message queue with a human-readable name for debugging
     * and tracing purposes. The name is truncated to fit within the internal
     * buffer (31 characters plus null terminator).
     *
     * @param name String identifier for the message queue (used for
     * debugging/tracing)
     * @note The name is stored in a fixed-size buffer and will be truncated if
     * too long
     */
    template <size_t N>
    explicit consteval IMessageQueue(const char (&name)[N]) noexcept {
        static_assert(N <= queue_name_size,
                      "Queue name exceeds maximum length");

        std::copy_n(name, N - 1, queue_name.begin());
        queue_name[N - 1] = '\0';
    }

    /**
     * @brief Constructs a message queue with a runtime-provided name.
     *
     * Initializes the message queue with a name from a pointer parameter.
     * The name is truncated to fit within the internal buffer (31 characters
     * plus null terminator). This constructor is used when the name cannot be
     * determined at compile time.
     *
     * @param name Non-null pointer to the queue name string
     * @note The name is stored in a fixed-size buffer and will be truncated if
     * too long
     */
    explicit constexpr IMessageQueue(gsl::not_null<const char*> name) noexcept {
        const size_t len = std::char_traits<char>::length(name.get());
        const size_t copy_len = std::min(len, queue_name_size - 1);
        std::copy_n(name.get(), copy_len, queue_name.begin());
        queue_name[copy_len] = '\0';
    }

    /**
     * @brief Copy constructor is deleted - message queues are non-copyable.
     */
    IMessageQueue(const IMessageQueue&) = delete;

    /**
     * @brief Copy assignment is deleted - message queues are non-copyable.
     */
    IMessageQueue& operator=(const IMessageQueue&) = delete;

    /**
     * @brief Move constructor is deleted - message queues are non-movable.
     */
    IMessageQueue(IMessageQueue&&) = delete;

    /**
     * @brief Move assignment is deleted - message queues are non-movable.
     */
    IMessageQueue& operator=(IMessageQueue&&) = delete;

    /**
     * @brief Initializes the message queue with the specified buffer and
     * capacity.
     *
     * Available only when the implementation requires caller-provided
     * storage. Delegates to the derived class's init_impl() to perform
     * platform-specific initialization. This method must be called before
     * using send() or receive().
     *
     * @param buffer Pointer to the storage buffer for messages
     * @param capacity Maximum number of messages the queue can hold
     * @return ErrorHandler with success (std::monostate) or MessageQueueError
     *
     * Example:
     * @code
     * auto result = queue.init(buffer, 10);
     * if (!result) {
     *     // Handle error: result.error()
     * }
     * @endcode
     */
    auto init(gsl::not_null<BufferType*> buffer, const std::uint32_t capacity)
        -> ErrorHandler<std::monostate, MessageQueueError>
        requires(RequiresBuffer)
    {
        if (capacity == 0U) {
            return ErrorHandler<std::monostate, MessageQueueError>(
                MessageQueueError::InvalidParameter, "Capacity cannot be zero");
        }
        return static_cast<Derived*>(this)->init_impl(buffer.get(), capacity);
    }

    /**
     * @brief Initializes the message queue without an external buffer.
     *
     * Enabled for implementations that manage their own storage (e.g., POSIX
     * named queues). Delegates to the derived class's init_impl() to perform
     * platform-specific initialization.
     *
     * @param capacity Maximum number of messages the queue can hold
     * @return ErrorHandler with success (std::monostate) or MessageQueueError
     *
     * Example:
     * @code
     * auto result = queue.init(10);
     * if (!result) {
     *     LOGGER_ERROR("Queue init failed: %d",
     * static_cast<int>(result.error()));
     * }
     * @endcode
     */
    auto init(const std::uint32_t capacity)
        -> ErrorHandler<std::monostate, MessageQueueError>
        requires(!RequiresBuffer)
    {
        if (capacity == 0U) {
            return ErrorHandler<std::monostate, MessageQueueError>(
                MessageQueueError::InvalidParameter, "Capacity cannot be zero");
        }
        return static_cast<Derived*>(this)->init_impl(capacity);
    }

    /**
     * @brief Sends a message to the queue with a specified timeout.
     *
     * Attempts to place a message into the queue. If the queue is full, the
     * calling thread blocks until either space becomes available or the timeout
     * expires. The message is copied into the queue's internal storage.
     *
     * @param message The message to send (passed by non-const reference)
     * @param timeout Maximum duration to wait for queue space availability
     * @return ErrorHandler with success (std::monostate) or MessageQueueError
     *
     * @note The queue must be initialized before calling this method
     *
     * Example:
     * @code
     * MyMessage msg{42};
     * queue.send(msg, 1000)
     *     .or_else([](auto err) {
     *         LOGGER_ERROR("Send failed");
     *         return ErrorHandler<std::monostate,
     * MessageQueueError>(std::monostate{});
     *     });
     * @endcode
     */
    auto send(T& message, DurationType timeout) const
        -> ErrorHandler<std::monostate, MessageQueueError> {
        return static_cast<const Derived*>(this)->send_impl(message, timeout);
    }

    /**
     * @brief Receives a message from the queue with a specified timeout.
     *
     * Attempts to retrieve a message from the queue in FIFO order. If the queue
     * is empty, the calling thread blocks until either a message becomes
     * available or the timeout expires. The received message is copied into the
     * provided reference.
     *
     * @param message Reference to store the received message
     * @param timeout Maximum duration to wait for message availability
     * @return ErrorHandler with success (std::monostate) or MessageQueueError
     *
     * @note The queue must be initialized before calling this method
     *
     * Example:
     * @code
     * MyMessage msg;
     * auto result = queue.receive(msg, 1000);
     * if (result) {
     *     // Process msg
     * } else if (result.error() == MessageQueueError::Timeout) {
     *     // Handle timeout
     * }
     * @endcode
     */
    auto receive(T& message, DurationType timeout) const
        -> ErrorHandler<std::monostate, MessageQueueError> {
        return static_cast<const Derived*>(this)->receive_impl(message,
                                                               timeout);
    }

    /**
     * @brief Queries the number of free slots available in the queue.
     *
     * Returns the current number of available message slots in the queue.
     * This can be used to check if the queue has space before attempting
     * a send operation or for monitoring queue utilization.
     *
     * @return Number of free message slots currently available
     * @note This value can change between checking and sending if other threads
     *       are accessing the queue concurrently
     */
    auto get_free_space() const -> size_t {
        return static_cast<const Derived*>(this)->get_free_space_impl();
    }

   protected:
    /**
     * @brief Default destructor.
     * @note Protected to prevent deletion through base class pointer.
     * @note Non-virtual as CRTP doesn't require runtime polymorphism.
     */
    ~IMessageQueue() = default;

    /**
     * @brief Get the message queue name for debugging purposes.
     * @return Pointer to the null-terminated queue name string.
     */
    auto get_name() -> char* { return queue_name.data(); }

   private:
    static constexpr std::size_t queue_name_size = 32;
    /**
     * @brief Fixed-size buffer storing the message queue name for debugging.
     * @note Maximum name length is 31 characters plus null terminator.
     */
    std::array<char, queue_name_size> queue_name{""};
};

}  // namespace PlatformCore