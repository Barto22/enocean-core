#include "Thread.hpp"

#include <cerrno>

namespace {
/**
 * @brief Helper function to convert Zephyr errno codes to ThreadError enum.
 */
constexpr auto errno_to_thread_error(int error_code)
    -> PlatformCore::ThreadError {
    using PlatformCore::ThreadError;
    switch (error_code) {
        case EINVAL:
            return ThreadError::InvalidParameter;
        case EAGAIN:
            return ThreadError::ResourceLimitExceeded;
        case ESRCH:
            return ThreadError::ThreadNotFound;
        case EALREADY:
            return ThreadError::AlreadyCreated;
        default:
            return ThreadError::Unknown;
    }
}
}  // namespace

namespace PlatformCore {

auto Thread::os_create(ThreadFunctionVariant threadFunction,
                       const ThreadStackInfo& stackInfo, int priority,
                       ThreadParamVariant& params)
    -> ErrorHandler<std::monostate, ThreadError> {
    if (!std::holds_alternative<ThreadFunction3>(threadFunction)) {
        return ErrorHandler<std::monostate, ThreadError>(
            ThreadError::InvalidParameter,
            "Invalid thread function type for Zephyr");
    }
    auto threadFun{std::get<ThreadFunction3>(threadFunction)};
    p1 = nullptr;
    p2 = nullptr;
    p3 = nullptr;
    if (std::holds_alternative<ThreadParam1>(params)) {
        const auto& param{std::get<ThreadParam1>(params).param};
        p1 = ThreadParamToPointer(param);
    } else if (std::holds_alternative<ThreadParam2>(params)) {
        const auto& param{std::get<ThreadParam2>(params)};
        p1 = ThreadParamToPointer(param.param1);
        p2 = ThreadParamToPointer(param.param2);
    } else if (std::holds_alternative<ThreadParam3>(params)) {
        const auto& param{std::get<ThreadParam3>(params)};
        p1 = ThreadParamToPointer(param.param1);
        p2 = ThreadParamToPointer(param.param2);
        p3 = ThreadParamToPointer(param.param3);
    } else if (std::holds_alternative<ThreadParamValue>(params)) {
        const auto raw_value{std::get<ThreadParamValue>(params).value};
        p1 = ThreadParamToPointer(raw_value);
    } else {
        return ErrorHandler<std::monostate, ThreadError>(
            ThreadError::InvalidParameter, "Invalid thread parameter type");
    }
    thread_id =
        k_thread_create(&thread, stackInfo.stack_pointer, stackInfo.stack_size,
                        threadFun, p1, p2, p3, priority, 0, K_NO_WAIT);

    const bool created{thread_id != nullptr};
    if (created) {
        set_active(true);
        return ErrorHandler<std::monostate, ThreadError>(std::monostate{});
    }

    return ErrorHandler<std::monostate, ThreadError>(
        ThreadError::CreationFailed, "k_thread_create failed");
}

auto Thread::os_create(ThreadFunctionVariant threadFunction,
                       const ThreadStackInfo& stackInfo, int priority)
    -> ErrorHandler<std::monostate, ThreadError> {
    if (!std::holds_alternative<ThreadFunction3>(threadFunction)) {
        return ErrorHandler<std::monostate, ThreadError>(
            ThreadError::InvalidParameter,
            "Invalid thread function type for Zephyr");
    }
    auto threadFun{std::get<ThreadFunction3>(threadFunction)};
    p1 = nullptr;
    p2 = nullptr;
    p3 = nullptr;
    thread_id = k_thread_create(&thread, stackInfo.stack_pointer,
                                stackInfo.stack_size, threadFun, nullptr,
                                nullptr, nullptr, priority, 0, K_NO_WAIT);

    const bool created{thread_id != nullptr};
    if (created) {
        set_active(true);
        return ErrorHandler<std::monostate, ThreadError>(std::monostate{});
    }

    return ErrorHandler<std::monostate, ThreadError>(
        ThreadError::CreationFailed, "k_thread_create failed");
}

auto Thread::os_destroy() -> ErrorHandler<std::monostate, ThreadError> {
    const int err{k_thread_join(thread_id, K_FOREVER)};
    if (err == 0) {
        set_active(false);
        return ErrorHandler<std::monostate, ThreadError>(std::monostate{});
    }

    return ErrorHandler<std::monostate, ThreadError>(
        errno_to_thread_error(-err), "k_thread_join failed");
}

}  // namespace PlatformCore