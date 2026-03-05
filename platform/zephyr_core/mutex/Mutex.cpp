#include "Mutex.hpp"

#include <cerrno>

namespace PlatformCore {

namespace {
/**
 * @brief Helper function to convert Zephyr errno codes to MutexError enum.
 */
constexpr auto errno_to_mutex_error(int error_code) -> MutexError {
    switch (error_code) {
        case EINVAL:
            return MutexError::InvalidParameter;
        case ETIMEDOUT:
            [[fallthrough]];
        case EBUSY:
            return MutexError::Timeout;
        case EDEADLK:
            return MutexError::Deadlock;
        case EALREADY:
            return MutexError::AlreadyInitialized;
        default:
            return MutexError::Unknown;
    }
}
}  // namespace

auto Mutex::do_init() -> ErrorHandler<std::monostate, MutexError> {
    if (is_initialized()) {
        return ErrorHandler<std::monostate, MutexError>(
            MutexError::AlreadyInitialized, "Mutex already initialized");
    }

    const int result{k_mutex_init(&mtx)};
    if (result != 0) {
        return ErrorHandler<std::monostate, MutexError>(
            errno_to_mutex_error(-result), "k_mutex_init failed");
    }

    set_initialized(true);
    return ErrorHandler<std::monostate, MutexError>(std::monostate{});
}

auto Mutex::do_lock(k_timeout_t timeout)
    -> ErrorHandler<std::monostate, MutexError> {
    const int result{k_mutex_lock(&mtx, timeout)};
    if (result == 0) {
        return ErrorHandler<std::monostate, MutexError>(std::monostate{});
    }

    const auto error{errno_to_mutex_error(-result)};
    return ErrorHandler<std::monostate, MutexError>(error,
                                                    "k_mutex_lock failed");
}

void Mutex::do_unlock() { k_mutex_unlock(&mtx); }

}  // namespace PlatformCore