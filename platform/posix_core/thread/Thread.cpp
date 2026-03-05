#include "Thread.hpp"

#include <sched.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>

namespace {
/**
 * @brief Helper function to convert POSIX errno codes to ThreadError enum.
 */
constexpr auto errno_to_thread_error(int error_code)
    -> PlatformCore::ThreadError {
    using PlatformCore::ThreadError;
    switch (error_code) {
        case EINVAL:
            return ThreadError::InvalidParameter;
        case ENOMEM:
            return ThreadError::InsufficientMemory;
        case EAGAIN:
            return ThreadError::ResourceLimitExceeded;
        case EPERM:
            return ThreadError::PermissionDenied;
        case ESRCH:
            return ThreadError::ThreadNotFound;
        case EDEADLK:
            return ThreadError::Deadlock;
        case EALREADY:
            return ThreadError::AlreadyCreated;
        default:
            return ThreadError::Unknown;
    }
}
auto configure_priority(pthread_attr_t& attr, int priority) -> int {
    if (priority < 0) {
        (void)pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
        return 0;
    }

    constexpr int policy{SCHED_FIFO};
    const int min_priority{sched_get_priority_min(policy)};
    const int max_priority{sched_get_priority_max(policy)};
    if ((min_priority == -1) || (max_priority == -1)) {
        (void)pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
        return -EINVAL;
    }

    const int clamped{std::clamp(priority, min_priority, max_priority)};
    sched_param sched{};
    sched.sched_priority = clamped;

    const bool policy_ok{pthread_attr_setschedpolicy(&attr, policy) == 0};
    const bool param_ok{policy_ok &&
                        (pthread_attr_setschedparam(&attr, &sched) == 0)};
    const bool explicit_ok{
        param_ok &&
        (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) == 0)};

    if (policy_ok && param_ok && explicit_ok) {
        return 0;
    }

    // Fallback to inherited scheduling when real-time priority is not allowed.
    (void)pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
    (void)pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
    return -EPERM;
}

auto minimum_stack_size() -> std::size_t {
    constexpr std::size_t fallback{16384U};
#ifdef PTHREAD_STACK_MIN
    auto min_stack{static_cast<std::size_t>(PTHREAD_STACK_MIN)};
    if (min_stack == 0U) {
        min_stack = fallback;
    }
    return min_stack;
#else
#ifdef _SC_THREAD_STACK_MIN
    const auto runtime_min{sysconf(_SC_THREAD_STACK_MIN)};
    if (runtime_min > 0) {
        return static_cast<std::size_t>(runtime_min);
    }
#endif
    return fallback;
#endif
}

auto sanitize_stack_size(std::size_t requested) -> std::size_t {
    const auto min_stack{minimum_stack_size()};
    if (requested == 0U) {
        return min_stack;
    }
    const auto word_align{sizeof(void*) - 1U};
    const auto aligned{(requested + word_align) & ~word_align};
    return std::max(aligned, min_stack);
}
}  // namespace

namespace PlatformCore {

auto Thread::os_create(ThreadFunctionVariant threadFunction,
                       const ThreadStackInfo& stackInfo, int priority,
                       ThreadParamVariant& params)
    -> ErrorHandler<std::monostate, ThreadError> {
    const auto stack_size{sanitize_stack_size(stackInfo.stack_size)};

    param = nullptr;
    if (std::holds_alternative<ThreadParamValue>(params)) {
        const auto raw_value{std::get<ThreadParamValue>(params).value};
        param = ThreadParamToPointer(raw_value);
    } else if (std::holds_alternative<ThreadParam1>(params)) {
        param = ThreadParamToPointer(std::get<ThreadParam1>(params).param);
    } else {
        return ErrorHandler<std::monostate, ThreadError>(
            ThreadError::InvalidParameter, "Invalid thread parameter type");
    }

    auto attempt_create{[&](bool force_inherit) -> int {
        if (const int init_err{pthread_attr_init(&attr)}; init_err != 0) {
            return init_err;
        }

        auto destroy_attr{[this]() -> bool {
            const int err{pthread_attr_destroy(&attr)};
            return (err == 0);
        }};

        if (const int stack_err{pthread_attr_setstacksize(&attr, stack_size)};
            stack_err != 0) {
            (void)destroy_attr();
            return stack_err;
        }

        if (force_inherit) {
            (void)pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
            (void)pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
        } else {
            [[maybe_unused]] const int priority_result{
                configure_priority(attr, priority)};
        }

        if (!std::holds_alternative<ThreadFunction1>(threadFunction)) {
            (void)destroy_attr();
            return EINVAL;
        }

        auto threadFun{std::get<ThreadFunction1>(threadFunction)};
        const int err{pthread_create(&thread, &attr, threadFun, param)};

        (void)destroy_attr();
        return err;
    }};

    auto err{attempt_create(false)};
    if ((err == EPERM) && (priority >= 0)) {
        err = attempt_create(true);
    }

    if (err == 0) {
        set_active(true);
        return ErrorHandler<std::monostate, ThreadError>(std::monostate{});
    }

    return ErrorHandler<std::monostate, ThreadError>(errno_to_thread_error(err),
                                                     "pthread_create failed");
}

auto Thread::os_create(ThreadFunctionVariant threadFunction,
                       const ThreadStackInfo& stackInfo, int priority)
    -> ErrorHandler<std::monostate, ThreadError> {
    const auto stack_size{sanitize_stack_size(stackInfo.stack_size)};
    param = nullptr;

    auto attempt_create{[&](bool force_inherit) -> int {
        if (const int init_err{pthread_attr_init(&attr)}; init_err != 0) {
            return init_err;
        }

        auto destroy_attr{[this]() -> bool {
            const int err{pthread_attr_destroy(&attr)};
            return (err == 0);
        }};

        if (const int stack_err{pthread_attr_setstacksize(&attr, stack_size)};
            stack_err != 0) {
            (void)destroy_attr();
            return stack_err;
        }

        if (force_inherit) {
            (void)pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
            (void)pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
        } else {
            [[maybe_unused]] const int priority_result{
                configure_priority(attr, priority)};
        }

        if (!std::holds_alternative<ThreadFunction1>(threadFunction)) {
            (void)destroy_attr();
            return EINVAL;
        }

        auto threadFun{std::get<ThreadFunction1>(threadFunction)};
        const int err{pthread_create(&thread, &attr, threadFun, nullptr)};

        (void)destroy_attr();
        return err;
    }};

    auto err{attempt_create(false)};
    if ((err == EPERM) && (priority >= 0)) {
        err = attempt_create(true);
    }

    if (err == 0) {
        set_active(true);
        return ErrorHandler<std::monostate, ThreadError>(std::monostate{});
    }

    return ErrorHandler<std::monostate, ThreadError>(errno_to_thread_error(err),
                                                     "pthread_create failed");
}

auto Thread::os_destroy() -> ErrorHandler<std::monostate, ThreadError> {
    int err{0};
    do {
        err = pthread_join(thread, nullptr);
    } while (err == EINTR);

    if (err == 0) {
        set_active(false);
        return ErrorHandler<std::monostate, ThreadError>(std::monostate{});
    }
    return ErrorHandler<std::monostate, ThreadError>(errno_to_thread_error(err),
                                                     "pthread_join failed");
}

}  // namespace PlatformCore