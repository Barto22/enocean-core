/**
 * @file Semaphore.hpp
 * @brief Zephyr RTOS semaphore implementation for platform semaphore
 * abstraction.
 *
 * This file provides a concrete implementation of the ISemaphore interface
 * using Zephyr k_sem primitives. Supports both blocking and timed semaphore
 * operations with k_timeout_t based timeouts (K_NO_WAIT, K_FOREVER, K_MSEC,
 * etc.).
 *
 * - `Semaphore` is a concrete implementation of `ISemaphore` for Zephyr RTOS.
 *
 * Usage:
 *   - Use `Semaphore` for counting semaphore operations in Zephyr-based
 *     applications.
 *   - Initialize with desired initial count and maximum limit.
 */
#pragma once

#include <zephyr/kernel.h>

#include "ISemaphore.hpp"

/**
 * @namespace PlatformCore
 * @brief Root namespace for platform abstraction interfaces and utilities.
 */
namespace PlatformCore {

/**
 * @brief Zephyr RTOS counting semaphore implementation using Zephyr kernel
 * primitives.
 *
 * This class provides a concrete implementation of the ISemaphore interface
 * using Zephyr's k_sem functionality. It supports both blocking and timed
 * semaphore operations through k_sem_take and k_sem_give functions with
 * k_timeout_t based timeouts.
 *
 * The semaphore is initialized during the do_init() call using k_sem_init with
 * both initial count and maximum limit enforcement. Unlike POSIX semaphores,
 * Zephyr semaphores enforce the maximum count limit specified during
 * initialization.
 *
 * @note This implementation is suitable for Zephyr RTOS environments.
 * @note Zephyr semaphores enforce maximum count limits to prevent overflow.
 * @see ISemaphore for the base interface requirements
 */
class Semaphore : public ISemaphore<Semaphore, k_timeout_t> {
   public:
    using ISemaphore<Semaphore, k_timeout_t>::ISemaphore;

    /**
     * @brief Initializes the Zephyr semaphore using k_sem_init.
     *
     * Creates a counting semaphore with the initial count and maximum limit
     * specified in the constructor. The semaphore enforces the maximum count;
     * k_sem_give will fail if the count would exceed this limit.
     *
     * @return ErrorHandler with success (std::monostate) or SemaphoreError
     * @note Must be called before any take() or give() operations
     */
    auto do_init() -> ErrorHandler<std::monostate, SemaphoreError>;

    /**
     * @brief Attempts to take (decrement) the semaphore with Zephyr timeout
     * specification.
     *
     * This method implements the CRTP interface requirement for do_take(). It
     * uses k_sem_take to acquire the semaphore with a timeout specified using
     * Zephyr's k_timeout_t type.
     *
     * Special timeout values:
     * - K_FOREVER: Blocks indefinitely until semaphore is available
     * - K_NO_WAIT: Returns immediately if semaphore cannot be acquired
     * - K_MSEC(ms)/K_USEC(us)/K_TICKS(t): Waits for specified duration
     *
     * If the semaphore count is greater than zero, it is decremented and the
     * operation succeeds immediately. Otherwise, the calling thread blocks
     * until either the semaphore becomes available or the timeout expires.
     *
     * @param ticks Zephyr timeout specification for semaphore acquisition
     * @return ErrorHandler with success (std::monostate) or SemaphoreError
     */
    auto do_take(k_timeout_t ticks)
        -> ErrorHandler<std::monostate, SemaphoreError>;

    /**
     * @brief Gives (increments) the semaphore using k_sem_give.
     *
     * Increments the semaphore count (up to the maximum limit) and wakes one
     * waiting thread if any are blocked on k_sem_take. The operation never
     * blocks.
     *
     * @note If the semaphore count is at its maximum limit, k_sem_give has no
     *       effect and does not increment the count further (saturates at max)
     */
    void do_give();

   private:
    struct k_sem sem{};  ///< Underlying Zephyr semaphore handle
};

static_assert(SemaphoreImplementation<Semaphore, k_timeout_t>,
              "Zephyr Semaphore must implement the required interface");

}  // namespace PlatformCore