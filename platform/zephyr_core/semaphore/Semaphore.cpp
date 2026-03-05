#include "Semaphore.hpp"

#include <cerrno>

namespace PlatformCore {

namespace {
/**
 * @brief Helper function to convert Zephyr errno codes to SemaphoreError enum.
 */
constexpr auto errno_to_semaphore_error(int error_code) -> SemaphoreError {
    switch (error_code) {
        case EINVAL:
            return SemaphoreError::InvalidParameter;
        case ETIMEDOUT:
            [[fallthrough]];
        case EBUSY:
            [[fallthrough]];
        case EAGAIN:
            return SemaphoreError::Timeout;
        case EALREADY:
            return SemaphoreError::AlreadyInitialized;
        default:
            return SemaphoreError::Unknown;
    }
}
}  // namespace

auto Semaphore::do_init() -> ErrorHandler<std::monostate, SemaphoreError> {
    if (is_initialized()) {
        return ErrorHandler<std::monostate, SemaphoreError>(
            SemaphoreError::AlreadyInitialized,
            "Semaphore already initialized");
    }

    const int result{k_sem_init(&sem, get_initial_count(), get_count_limit())};
    if (result != 0) {
        return ErrorHandler<std::monostate, SemaphoreError>(
            errno_to_semaphore_error(-result), "k_sem_init failed");
    }

    set_initialized(true);
    return ErrorHandler<std::monostate, SemaphoreError>(std::monostate{});
}

auto Semaphore::do_take(k_timeout_t ticks)
    -> ErrorHandler<std::monostate, SemaphoreError> {
    const int result{k_sem_take(&sem, ticks)};
    if (result == 0) {
        return ErrorHandler<std::monostate, SemaphoreError>(std::monostate{});
    }

    // Map Zephyr timeout codes to SemaphoreError for uniformity
    const auto error{errno_to_semaphore_error(-result)};
    return ErrorHandler<std::monostate, SemaphoreError>(error,
                                                        "k_sem_take failed");
}

void Semaphore::do_give() { k_sem_give(&sem); }

}  // namespace PlatformCore