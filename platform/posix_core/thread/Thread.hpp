
/**
 * @file Thread.hpp
 * @brief POSIX thread implementation for platform thread abstraction.
 *
 * This file defines the POSIX-specific thread class (`Thread`) implementing the
 * core thread interface (`IThread`).
 *
 * - `Thread` is a concrete implementation of `IThread` for POSIX platforms.
 * - `ThreadImplementation` concept checks for valid thread implementations.
 *
 * Usage:
 *   - Use `Thread` for thread management in POSIX-based applications.
 *   - Pass a `Thread` instance to platform code for portable thread operations.
 */
#pragma once

#include <pthread.h>

#include "IThread.hpp"

/**
 * @namespace PlatformCore
 * @brief Root namespace for platform abstraction interfaces and utilities.
 */
namespace PlatformCore {

/**
 * @class Thread
 * @brief POSIX-specific thread implementation.
 *
 * Implements the `IThread` interface for POSIX platforms, providing thread
 * creation, management, and destruction via POSIX mechanisms (e.g., pthreads).
 */
class Thread final : public IThread<Thread, void*> {
   public:
    using IThread<Thread, void*>::IThread;

    /**
     * @brief Virtual destructor for safe polymorphic deletion.
     */
    ~Thread() noexcept {
        if (is_active()) {
            // Explicitly ignore return value - best effort cleanup in
            // destructor
            (void)destroy();
        }
    }

    /**
     * @brief Deleted copy constructor.
     */
    Thread(const Thread&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    Thread& operator=(const Thread&) = delete;

    /**
     * @brief Deleted move constructor.
     */
    Thread(Thread&&) = delete;

    /**
     * @brief Deleted move assignment operator.
     */
    Thread& operator=(Thread&&) = delete;

    /**
     * @brief Create and start the POSIX thread.
     * @param threadFunction Thread entry function, @ref ThreadFunctionVariant.
     * @param stackInfo Stack pointer and size information.
     * @param priority Thread priority.
     * @param params Argument for thread entry, @ref ThreadParamVariant.
     * @return ErrorHandler with success (std::monostate) or ThreadError
     */
    auto os_create(ThreadFunctionVariant threadFunction,
                   const ThreadStackInfo& stackInfo, int priority,
                   ThreadParamVariant& params)
        -> ErrorHandler<std::monostate, ThreadError>;

    /**
     * @brief Create and start the POSIX thread without param.
     * @param threadFunction Thread entry function, @ref ThreadFunctionVariant.
     * @param stackInfo Stack pointer and size information.
     * @param priority Thread priority.
     * @return ErrorHandler with success (std::monostate) or ThreadError
     */
    auto os_create(ThreadFunctionVariant threadFunction,
                   const ThreadStackInfo& stackInfo, int priority)
        -> ErrorHandler<std::monostate, ThreadError>;

    /**
     * @brief Destroy the POSIX thread and release resources.
     * @return ErrorHandler with success (std::monostate) or ThreadError
     */
    auto os_destroy() -> ErrorHandler<std::monostate, ThreadError>;

   private:
    void* param{};
    pthread_t thread{};
    pthread_attr_t attr{};
};

/**
 * @brief Compile-time check for valid thread implementation.
 */
static_assert(
    ThreadImplementation<Thread, void*>,
    "The Posix Thread must correctly implement the IThread interface.");

}  // namespace PlatformCore