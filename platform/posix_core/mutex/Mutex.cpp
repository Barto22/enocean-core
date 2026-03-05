#include "Mutex.hpp"

#include <cerrno>
#include <exception>

namespace PlatformCore {

namespace {
/**
 * @brief Helper function to convert POSIX errno codes to MutexError enum.
 */
constexpr auto errno_to_mutex_error(int error_code) -> MutexError {
    switch (error_code) {
        case EINVAL:
            return MutexError::InvalidParameter;
        case ENOMEM:
            return MutexError::InsufficientMemory;
        case EAGAIN:
            return MutexError::ResourceLimitExceeded;
        case ETIMEDOUT:
            return MutexError::Timeout;
        case EINTR:
            return MutexError::Interrupted;
        case EDEADLK:
            return MutexError::Deadlock;
        case EBUSY:
            return MutexError::Busy;
        case EPERM:
            return MutexError::PermissionDenied;
        case EALREADY:
            return MutexError::AlreadyInitialized;
        default:
            return MutexError::Unknown;
    }
}
}  // namespace

auto Mutex::do_init() -> ErrorHandler<std::monostate, MutexError> {
    if (is_initialized()) {
        if (const int destroy_rc{pthread_mutex_destroy(&mtx)};
            destroy_rc != 0) {
            return ErrorHandler<std::monostate, MutexError>(
                errno_to_mutex_error(destroy_rc),
                "pthread_mutex_destroy failed");
        }
        set_initialized(false);
    }

    pthread_mutexattr_t attr{};
    if (const int attr_init_rc{pthread_mutexattr_init(&attr)};
        attr_init_rc != 0) {
        return ErrorHandler<std::monostate, MutexError>(
            errno_to_mutex_error(attr_init_rc),
            "pthread_mutexattr_init failed");
    }

    if (const auto set_type_rc{
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK)};
        set_type_rc != 0) {
        (void)pthread_mutexattr_destroy(&attr);
        return ErrorHandler<std::monostate, MutexError>(
            errno_to_mutex_error(set_type_rc),
            "pthread_mutexattr_settype failed");
    }

    const int init_rc{pthread_mutex_init(&mtx, &attr)};
    const int attr_destroy_rc{pthread_mutexattr_destroy(&attr)};

    if (attr_destroy_rc != 0) {
        if (init_rc == 0) {
            (void)pthread_mutex_destroy(&mtx);
        }
        return ErrorHandler<std::monostate, MutexError>(
            errno_to_mutex_error(attr_destroy_rc),
            "pthread_mutexattr_destroy failed");
    }

    if (init_rc != 0) {
        return ErrorHandler<std::monostate, MutexError>(
            errno_to_mutex_error(init_rc), "pthread_mutex_init failed");
    }

    set_initialized(true);
    return ErrorHandler<std::monostate, MutexError>(std::monostate{});
}

auto Mutex::do_lock(const struct timespec* abs_timeout)
    -> ErrorHandler<std::monostate, MutexError> {
    const int result{(abs_timeout == nullptr)
                         ? pthread_mutex_lock(&mtx)
                         : pthread_mutex_timedlock(&mtx, abs_timeout)};

    if (result == 0) {
        return ErrorHandler<std::monostate, MutexError>(std::monostate{});
    }

    return ErrorHandler<std::monostate, MutexError>(
        errno_to_mutex_error(result), (abs_timeout == nullptr)
                                          ? "pthread_mutex_lock failed"
                                          : "pthread_mutex_timedlock failed");
}

void Mutex::do_unlock() {
    if (pthread_mutex_unlock(&mtx) != 0) {
        std::terminate();
    }
}

}  // namespace PlatformCore