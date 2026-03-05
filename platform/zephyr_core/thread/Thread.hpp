
/**
 * @file Thread.hpp
 * @brief Zephyr thread implementation for platform thread abstraction.
 *
 * This file defines the Zephyr-specific thread class (`Thread`) implementing
 * the core thread interface (`IThread`).
 *
 * - `Thread` is a concrete implementation of `IThread` for Zephyr platforms.
 * - `ThreadImplementation` concept checks for valid thread implementations.
 *
 * Usage:
 *   - Use `Thread` for thread management in Zephyr-based applications.
 *   - Pass a `Thread` instance to platform code for portable thread operations.
 */
#pragma once

#include <zephyr/kernel.h>

#include "IThread.hpp"

namespace PlatformCore {

/**
 * @class Thread
 * @brief Zephyr-specific thread implementation.
 *
 * Implements the `IThread` interface for Zephyr platforms, providing thread
 * creation, management, and destruction via Zephyr mechanisms.
 */
class Thread final : public IThread<Thread, k_thread_stack_t*> {
   public:
    using IThread<Thread, k_thread_stack_t*>::IThread;

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
     * @brief Create and start the Zephyr thread.
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
     * @brief Create and start the Zephyr thread without param.
     * @param threadFunction Thread entry function, @ref ThreadFunctionVariant.
     * @param stackInfo Stack pointer and size information.
     * @param priority Thread priority.
     * @return ErrorHandler with success (std::monostate) or ThreadError
     */
    auto os_create(ThreadFunctionVariant threadFunction,
                   const ThreadStackInfo& stackInfo, int priority)
        -> ErrorHandler<std::monostate, ThreadError>;

    /**
     * @brief Destroy the Zephyr thread and release resources.
     * @return ErrorHandler with success (std::monostate) or ThreadError
     */
    auto os_destroy() -> ErrorHandler<std::monostate, ThreadError>;

   private:
    void* p1{};
    void* p2{};
    void* p3{};
    k_thread thread{};
    k_tid_t thread_id{};
};

/**
 * @brief Compile-time check for valid thread implementation.
 */
static_assert(
    ThreadImplementation<Thread, k_thread_stack_t*>,
    "The Zephyr Thread must correctly implement the IThread interface.");

}  // namespace PlatformCore