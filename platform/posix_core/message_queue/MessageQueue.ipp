/**
 * @file MessageQueue.ipp
 * @brief POSIX MessageQueue<T> template method definitions.
 *
 * This file contains the out-of-line definitions for all template methods of
 * MessageQueue<T>. It is included at the bottom of MessageQueue.hpp and must
 * not be included directly by other translation units.
 */
#pragma once

namespace PlatformCore {

template <typename T>
MessageQueue<T>::~MessageQueue() {
    if (mqd_ != static_cast<mqd_t>(-1)) {
        mq_close(mqd_);
        const auto* name{this->get_name()};
        if ((name != nullptr) && (name[0] != '\0')) {
            mq_unlink(name);
        }
    }
}

template <typename T>
auto MessageQueue<T>::init_impl(const std::uint32_t capacity)
    -> ErrorHandler<std::monostate, MessageQueueError> {
    struct mq_attr attr{};
    attr.mq_flags = 0;
    attr.mq_maxmsg = static_cast<long>(capacity);
    attr.mq_msgsize = static_cast<long>(sizeof(T));
    attr.mq_curmsgs = 0;

    auto* queue_name{this->get_name()};
    if ((queue_name == nullptr) || (queue_name[0] == '\0')) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            MessageQueueError::InvalidParameter, "Queue name is empty");
    }

    // POSIX message queue names must start with '/'
    if (queue_name[0] != '/') {
        return ErrorHandler<std::monostate, MessageQueueError>(
            MessageQueueError::InvalidParameter,
            "Queue name must start with '/'");
    }

    mqd_ = mq_open(queue_name, O_CREAT | O_RDWR, 0644, &attr);

    if (mqd_ == static_cast<mqd_t>(-1)) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            errno_to_error(errno), "mq_open failed");
    }
    return ErrorHandler<std::monostate, MessageQueueError>(std::monostate{});
}

template <typename T>
auto MessageQueue<T>::send_impl(T& msg, std::uint32_t timeout_ms) const
    -> ErrorHandler<std::monostate, MessageQueueError> {
    if (mqd_ == static_cast<mqd_t>(-1)) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            MessageQueueError::InvalidQueue, "Queue not initialized");
    }

    constexpr std::uint32_t infinite_timeout{
        std::numeric_limits<std::uint32_t>::max()};
    int result{0};
    if ((timeout_ms == 0U) || (timeout_ms == infinite_timeout)) {
        result =
            mq_send(mqd_, reinterpret_cast<const char*>(&msg), sizeof(T), 0U);
    } else {
        auto ts{ms_to_timespec(timeout_ms)};
        result = mq_timedsend(mqd_, reinterpret_cast<const char*>(&msg),
                              sizeof(T), 0U, &ts);
    }

    if (result != 0) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            errno_to_error(errno), "mq_send/mq_timedsend failed");
    }
    return ErrorHandler<std::monostate, MessageQueueError>(std::monostate{});
}

template <typename T>
auto MessageQueue<T>::receive_impl(T& msg, std::uint32_t timeout_ms) const
    -> ErrorHandler<std::monostate, MessageQueueError> {
    if (mqd_ == static_cast<mqd_t>(-1)) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            MessageQueueError::InvalidQueue, "Queue not initialized");
    }

    constexpr std::uint32_t infinite_timeout{
        std::numeric_limits<std::uint32_t>::max()};
    ssize_t bytes_read{0};
    if ((timeout_ms == 0U) || (timeout_ms == infinite_timeout)) {
        bytes_read =
            mq_receive(mqd_, reinterpret_cast<char*>(&msg), sizeof(T), nullptr);
    } else {
        auto ts{ms_to_timespec(timeout_ms)};
        bytes_read = mq_timedreceive(mqd_, reinterpret_cast<char*>(&msg),
                                     sizeof(T), nullptr, &ts);
    }

    if (bytes_read < 0) {
        return ErrorHandler<std::monostate, MessageQueueError>(
            errno_to_error(errno), "mq_receive/mq_timedreceive failed");
    }
    return ErrorHandler<std::monostate, MessageQueueError>(std::monostate{});
}

template <typename T>
auto MessageQueue<T>::get_free_space_impl() const -> size_t {
    if (mqd_ == static_cast<mqd_t>(-1)) {
        return 0;
    }
    struct mq_attr attr{};
    if (0 == mq_getattr(mqd_, &attr)) {
        return static_cast<size_t>(attr.mq_maxmsg - attr.mq_curmsgs);
    }
    return 0;
}

template <typename T>
auto MessageQueue<T>::ms_to_timespec(std::uint32_t timeout_ms) const
    -> struct timespec {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);

    const auto max_sec{std::numeric_limits<time_t>::max()};
    const auto now_sec{ts.tv_sec};
    const auto add_sec{static_cast<time_t>(timeout_ms / 1000U)};
    const auto add_nsec{static_cast<long>((timeout_ms % 1000U) * 1000000U)};

    if ((max_sec - now_sec) <= 0) {
        ts.tv_sec = max_sec;
        ts.tv_nsec = 999999999L;
        return ts;
    }

    const auto safe_sec{(add_sec > (max_sec - now_sec)) ? (max_sec - now_sec)
                                                        : add_sec};
    ts.tv_sec += safe_sec;
    ts.tv_nsec += add_nsec;
    if (ts.tv_nsec >= 1000000000L) {
        if (ts.tv_sec < max_sec) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        } else {
            ts.tv_nsec = 999999999L;
        }
    }

    return ts;
}

}  // namespace PlatformCore
