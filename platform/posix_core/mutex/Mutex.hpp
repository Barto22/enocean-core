/**
 * @file Mutex.hpp
 * @brief POSIX mutex implementation for platform mutex abstraction.
 *
 * This file provides a concrete implementation of the IMutex interface using
 * POSIX pthread_mutex primitives. Supports both blocking and timed mutex
 * acquisition operations with CLOCK_REALTIME based timeouts.
 *
 * - `Mutex` is a concrete implementation of `IMutex` for POSIX platforms.
 * - `MutexLocker` provides RAII-style automatic lock management.
 *
 * Usage:
 *   - Use `Mutex` for mutual exclusion in POSIX-based applications.
 *   - Use `MutexLocker` for exception-safe automatic lock/unlock.
 */
#pragma once

#include <pthread.h>

#include <ctime>

#include "IMutex.hpp"

/**
 * @namespace PlatformCore
 * @brief Root namespace for platform abstraction interfaces and utilities.
 */
namespace PlatformCore {

/**
 * @brief POSIX-based mutex implementation using pthread primitives
 *
 * This class provides a concrete implementation of the IMutex interface using
 * POSIX pthread_mutex functionality. It supports both blocking and timed mutex
 * acquisition operations through the pthread_mutex_lock and
 * pthread_mutex_timedlock functions.
 *
 * The mutex is initialized using PTHREAD_MUTEX_INITIALIZER and follows standard
 * pthread semantics for locking and unlocking.
 *
 * @note This implementation is thread-safe and suitable for POSIX-compliant
 * systems.
 * @see IMutex for the base interface requirements
 */
class Mutex : public IMutex<Mutex, const struct timespec*> {
   public:
    using IMutex<Mutex, const struct timespec*>::IMutex;

    /**
     * @brief Initializes (or re-initializes) the POSIX mutex with
     * error-checking semantics.
     *
     * The mutex is explicitly destroyed when it was previously initialized
     * and then recreated with pthread mutex attributes configured for
     * PTHREAD_MUTEX_ERRORCHECK. This guarantees deterministic runtime
     * behavior and reliable error reporting instead of relying on static
     * initializers.
     *
     * @return ErrorHandler with success (std::monostate) or MutexError
     * @note Calling init() on a locked mutex will fail to avoid
     *       undefined behavior.
     */
    auto do_init() -> ErrorHandler<std::monostate, MutexError>;

    /**
     * @brief Attempts to acquire the mutex with optional timeout
     *
     * This method implements the CRTP interface requirement for do_lock(). It
     * supports two modes of operation:
     * - If abs_timeout is nullptr: Blocks indefinitely until the mutex is
     * acquired
     * - If abs_timeout is provided: Waits until the specified absolute time
     *
     * Uses pthread_mutex_lock for blocking acquisition and
     * pthread_mutex_timedlock for timed acquisition.
     *
     * @param abs_timeout Pointer to absolute time specification
     * (CLOCK_REALTIME) for timeout, or nullptr for indefinite blocking
     * @return ErrorHandler with success (std::monostate) or MutexError
     */
    auto do_lock(const struct timespec* abs_timeout)
        -> ErrorHandler<std::monostate, MutexError>;

    /**
     * @brief Releases the previously acquired mutex
     *
     * This method implements the CRTP interface requirement for do_unlock().
     * Unlocks the mutex using pthread_mutex_unlock, allowing other threads to
     * acquire it.
     *
     * @warning Must only be called by the thread that successfully acquired the
     * mutex. Calling unlock from a different thread results in undefined
     * behavior.
     */
    void do_unlock();

   private:
    pthread_mutex_t
        mtx{};  ///< Underlying POSIX mutex handle (zero-initialised; do_init()
                ///< must be called before use)
};

static_assert(MutexImplementation<Mutex, const struct timespec*>);

/**
 * @brief POSIX-specific RAII mutex lock guard for automatic Mutex lifetime
 * management
 *
 * MutexLocker is a type alias that provides a concrete instantiation of the
 * IMutexLocker template for POSIX Mutex objects. It inherits all the RAII
 * functionality from IMutexLocker, ensuring automatic mutex acquisition on
 * construction and release on destruction.
 *
 * This class uses POSIX-specific timeout semantics through struct timespec*,
 * where the timeout parameter passed to the constructor must be either:
 * - nullptr for indefinite blocking (waits forever until lock is acquired)
 * - A pointer to struct timespec representing an absolute timeout based on
 *   CLOCK_REALTIME
 *
 * The locker is marked final to prevent further derivation and follows the
 * RAII idiom to provide exception-safe mutex handling in POSIX environments.
 *
 * @note This class is non-copyable and non-movable
 * @note The Mutex object must outlive the MutexLocker instance
 * @note For timed locks, use clock_gettime(CLOCK_REALTIME) to calculate
 *       absolute timeout values
 *
 * Example usage:
 * @code
 * PlatformCore::Mutex my_mutex("resource_mutex");
 * my_mutex.init();
 *
 * {
 *     // Blocking lock (wait indefinitely)
 *     PlatformCore::MutexLocker locker(my_mutex, nullptr);
 *     if (locker) {
 *         // Critical section - mutex is held
 *         // Mutex automatically unlocked when locker goes out of scope
 *     }
 * }
 *
 * {
 *     // Timed lock with absolute timeout
 *     struct timespec abs_timeout;
 *     clock_gettime(CLOCK_REALTIME, &abs_timeout);
 *     abs_timeout.tv_sec += 2; // Wait up to 2 seconds
 *
 *     PlatformCore::MutexLocker locker(my_mutex, &abs_timeout);
 *     if (locker) {
 *         // Critical section
 *     } else {
 *         // Timeout expired
 *     }
 * }
 * @endcode
 *
 * @see IMutexLocker for the base RAII lock guard template
 * @see Mutex for the POSIX mutex implementation
 */
class MutexLocker final : public IMutexLocker<const struct timespec*, Mutex> {
   public:
    using IMutexLocker<const struct timespec*, Mutex>::IMutexLocker;
};

}  // namespace PlatformCore