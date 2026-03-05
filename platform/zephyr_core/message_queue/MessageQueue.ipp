/**
 * @file MessageQueue.ipp
 * @brief Zephyr MessageQueue<T> template method definitions.
 *
 * This file contains the out-of-line definitions for all template methods of
 * MessageQueue<T>. It is included at the bottom of MessageQueue.hpp and must
 * not be included directly by other translation units.
 */
#pragma once

namespace PlatformCore {

template <typename T>
auto MessageQueue<T>::init_impl(char* buffer, const std::uint32_t capacity)
    -> ErrorHandler<std::monostate, MessageQueueError> {
    if (capacity == 0U) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            MessageQueueError::InvalidParameter, "Invalid capacity");
    }

    if (initialized_) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            MessageQueueError::AlreadyInitialized, "Queue already initialized");
    }

    k_msgq_init(&queue_, buffer, sizeof(T), capacity);
    initialized_ = true;
    return ErrorHandler<std::monostate, MessageQueueError>(std::monostate{});
}

template <typename T>
auto MessageQueue<T>::send_impl(T& msg, k_timeout_t timeout) const
    -> ErrorHandler<std::monostate, MessageQueueError> {
    if (!initialized_) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            MessageQueueError::InvalidQueue, "Queue not initialized");
    }

    const int result{k_msgq_put(&queue_, &msg, timeout)};
    if (result == 0) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            std::monostate{});
    }

    if ((result == -EAGAIN) || (result == -EBUSY)) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            MessageQueueError::QueueFull, "k_msgq_put queue full or timeout");
    }
    return ErrorHandler<std::monostate, MessageQueueError>(
        k_error_to_error(result), "k_msgq_put failed");
}

template <typename T>
auto MessageQueue<T>::receive_impl(T& msg, k_timeout_t timeout) const
    -> ErrorHandler<std::monostate, MessageQueueError> {
    if (!initialized_) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            MessageQueueError::InvalidQueue, "Queue not initialized");
    }

    const int result{k_msgq_get(&queue_, &msg, timeout)};
    if (result == 0) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            std::monostate{});
    }

    if ((result == -EAGAIN) || (result == -EBUSY)) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            MessageQueueError::QueueEmpty, "k_msgq_get queue empty or timeout");
    }
    return ErrorHandler<std::monostate, MessageQueueError>(
        k_error_to_error(result), "k_msgq_get failed");
}

template <typename T>
auto MessageQueue<T>::get_free_space_impl() const -> size_t {
    if (!initialized_) {
        return 0;
    }
    return k_msgq_num_free_get(const_cast<struct k_msgq*>(&queue_));
}

}  // namespace PlatformCore
