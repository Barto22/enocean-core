#include "Semaphore.hpp"

#include <cerrno>
#include <exception>

namespace PlatformCore {

namespace {
/**
 * @brief Helper function to convert POSIX errno codes to SemaphoreError enum.
 */
constexpr auto errno_to_semaphore_error(int error_code) -> SemaphoreError {
    switch (error_code) {
        case EINVAL:
            return SemaphoreError::InvalidParameter;
        case ENOMEM:
            return SemaphoreError::InsufficientMemory;
        case ENOSPC:
            return SemaphoreError::ResourceLimitExceeded;
        case ETIMEDOUT:
            return SemaphoreError::Timeout;
        case EINTR:
            return SemaphoreError::Interrupted;
        case EACCES:
            return SemaphoreError::PermissionDenied;
        case EOVERFLOW:
            return SemaphoreError::CountOverflow;
        case EALREADY:
            return SemaphoreError::AlreadyInitialized;
        default:
            return SemaphoreError::Unknown;
    }
}
}  // namespace

auto Semaphore::do_init() -> ErrorHandler<std::monostate, SemaphoreError> {
    if (is_initialized()) {
        if (const int destroy_result{sem_destroy(&sem)}; destroy_result != 0) {
            const int err{errno};
            return ErrorHandler<std::monostate, SemaphoreError>(
                errno_to_semaphore_error(err), "sem_destroy failed");
        }
        set_initialized(false);
    }

    if (const int result{sem_init(&sem, 0, get_initial_count())}; result == 0) {
        set_initialized(true);
        return ErrorHandler<std::monostate, SemaphoreError>(std::monostate{});
    }
    const int err{errno};
    return ErrorHandler<std::monostate, SemaphoreError>(
        errno_to_semaphore_error(err), "sem_init failed");
}

auto Semaphore::do_take(const struct timespec* abs_timeout)
    -> ErrorHandler<std::monostate, SemaphoreError> {
    const int result{(abs_timeout == nullptr)
                         ? sem_wait(&sem)
                         : sem_timedwait(&sem, abs_timeout)};

    if (result == 0) {
        return ErrorHandler<std::monostate, SemaphoreError>(std::monostate{});
    }

    const int err{errno};
    return ErrorHandler<std::monostate, SemaphoreError>(
        errno_to_semaphore_error(err), "sem_wait/sem_timedwait failed");
}

void Semaphore::do_give() {
    if (sem_post(&sem) != 0) {
        std::terminate();
    }
}

}  // namespace PlatformCore