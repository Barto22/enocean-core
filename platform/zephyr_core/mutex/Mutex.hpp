/**
 * @file Mutex.hpp
 * @brief Zephyr RTOS mutex implementation for platform mutex abstraction.
 *
 * This file provides a concrete implementation of the IMutex interface using
 * Zephyr k_mutex primitives. Supports both blocking and timed mutex acquisition
 * operations with k_timeout_t based timeouts.
 *
 * - `Mutex` is a concrete implementation of `IMutex` for Zephyr RTOS.
 * - `MutexLocker` provides RAII-style automatic lock management.
 *
 * Usage:
 *   - Use `Mutex` for mutual exclusion in Zephyr-based applications.
 *   - Use `MutexLocker` for exception-safe automatic lock/unlock.
 */
#pragma once

#include <zephyr/kernel.h>

#include "IMutex.hpp"

/**
 * @namespace PlatformCore
 * @brief Root namespace for platform abstraction interfaces and utilities.
 */
namespace PlatformCore {

/**
 * @brief Zephyr RTOS mutex implementation using Zephyr kernel primitives
 *
 * This class provides a concrete implementation of the IMutex interface using
 * Zephyr's k_mutex functionality. It supports both blocking and timed mutex
 * acquisition operations through the k_mutex_lock API with k_timeout_t based
 * timeouts.
 *
 * The mutex is initialized during construction via k_mutex_init. Zephyr mutexes
 * provide priority inheritance by default to prevent priority inversion issues.
 *
 * @note This implementation is suitable for Zephyr RTOS environments.
 * @note Zephyr mutexes support priority inheritance and owner tracking.
 * @see IMutex for the base interface requirements
 */
class Mutex : public IMutex<Mutex, k_timeout_t> {
   public:
    using IMutex<Mutex, k_timeout_t>::IMutex;

    /**
     * @brief Initializes the Zephyr mutex using k_mutex_init.
     *
     * Creates and initializes the Zephyr mutex with priority inheritance
     * enabled by default. After successful initialization, the mutex is ready
     * for lock and unlock operations. Zephyr mutexes are lightweight and
     * provide automatic priority inheritance to prevent priority inversion.
     *
     * @return ErrorHandler with success (std::monostate) or MutexError
     * @note Must be called after construction before using lock() or unlock()
     * @note Zephyr mutexes don't require explicit destruction
     */
    auto do_init() -> ErrorHandler<std::monostate, MutexError>;

    /**
     * @brief Attempts to acquire the mutex with Zephyr timeout specification
     *
     * This method implements the CRTP interface requirement for do_lock().
     * It uses k_mutex_lock to acquire the mutex with a timeout specified using
     * Zephyr's k_timeout_t type.
     *
     * Special timeout values:
     * - K_FOREVER: Blocks indefinitely until mutex is acquired
     * - K_NO_WAIT: Returns immediately if mutex cannot be acquired
     * - K_MSEC(ms)/K_USEC(us)/K_TICKS(t): Waits for specified duration
     *
     * @param ticks Zephyr timeout specification for mutex acquisition
     * @return ErrorHandler with success (std::monostate) or MutexError
     */
    auto do_lock(k_timeout_t ticks) -> ErrorHandler<std::monostate, MutexError>;

    /**
     * @brief Releases the previously acquired mutex
     *
     * This method implements the CRTP interface requirement for do_unlock().
     * Unlocks the mutex using k_mutex_unlock, allowing other waiting threads to
     * acquire it. If priority inheritance was active, thread priorities are
     * restored accordingly.
     *
     * @warning Must only be called by the thread that successfully acquired the
     *          mutex. Zephyr enforces ownership validation and will assert if
     *          unlock is attempted from a different thread.
     */
    void do_unlock();

   private:
    struct k_mutex mtx{};  ///< Underlying Zephyr mutex handle
};

static_assert(MutexImplementation<Mutex, k_timeout_t>);

/**
 * @brief Zephyr-specific RAII mutex lock guard for automatic Mutex lifetime
 * management
 *
 * MutexLocker is a concrete instantiation of the IMutexLocker template for
 * Zephyr Mutex objects. It provides RAII-based mutex management, ensuring
 * automatic mutex acquisition on construction and release on destruction,
 * following the Resource Acquisition Is Initialization idiom.
 *
 * This class uses Zephyr-specific timeout semantics through k_timeout_t,
 * which provides flexible timeout specifications. The timeout parameter passed
 * to the constructor can be:
 * - K_NO_WAIT: Returns immediately if mutex cannot be acquired
 * - K_FOREVER: Blocks indefinitely until mutex is acquired
 * - K_MSEC(ms): Waits for specified milliseconds (e.g., K_MSEC(100))
 * - K_USEC(us): Waits for specified microseconds (e.g., K_USEC(500))
 * - K_TICKS(t): Waits for specified system ticks
 * - K_SECONDS(s): Waits for specified seconds (e.g., K_SECONDS(5))
 *
 * The locker is marked final to prevent further derivation and ensures
 * exception-safe mutex handling in Zephyr RTOS environments with automatic
 * priority inheritance support.
 *
 * @note This class is non-copyable and non-movable
 * @note The Mutex object must outlive the MutexLocker instance
 * @note Zephyr provides priority inheritance by default to prevent priority
 * inversion
 * @note Zephyr enforces mutex ownership - only the acquiring thread can unlock
 *
 * Example usage:
 * @code
 * PlatformCore::Mutex my_mutex("resource_mutex");
 * my_mutex.init();
 *
 * {
 *     // Blocking lock (wait indefinitely)
 *     PlatformCore::MutexLocker locker(my_mutex, K_FOREVER);
 *     if (locker) {
 *         // Critical section - mutex is held
 *         // Mutex automatically unlocked when locker goes out of scope
 *     }
 * }
 *
 * {
 *     // Non-blocking lock attempt
 *     PlatformCore::MutexLocker locker(my_mutex, K_NO_WAIT);
 *     if (locker) {
 *         // Critical section
 *     } else {
 *         // Mutex was not available
 *     }
 * }
 *
 * {
 *     // Timed lock with 100ms timeout
 *     PlatformCore::MutexLocker locker(my_mutex, K_MSEC(100));
 *     if (locker.is_locked()) {
 *         // Critical section
 *     } else {
 *         // Timeout expired after 100ms
 *     }
 * }
 *
 * {
 *     // Timed lock with 2 second timeout
 *     PlatformCore::MutexLocker locker(my_mutex, K_SECONDS(2));
 *     if (locker) {
 *         // Critical section
 *     }
 * }
 * @endcode
 *
 * @see IMutexLocker for the base RAII lock guard template
 * @see Mutex for the Zephyr mutex implementation
 */
class MutexLocker final : public IMutexLocker<k_timeout_t, Mutex> {
   public:
    using IMutexLocker<k_timeout_t, Mutex>::IMutexLocker;
};

}  // namespace PlatformCore