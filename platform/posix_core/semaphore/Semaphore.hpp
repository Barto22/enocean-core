/**
 * @file Semaphore.hpp
 * @brief POSIX semaphore implementation for platform semaphore abstraction.
 *
 * This file provides a concrete implementation of the ISemaphore interface
 * using POSIX sem_t primitives. Supports both blocking and timed semaphore
 * operations with CLOCK_REALTIME based timeouts.
 *
 * - `Semaphore` is a concrete implementation of `ISemaphore` for POSIX
 * platforms.
 *
 * Usage:
 *   - Use `Semaphore` for counting semaphore operations in POSIX-based
 *     applications.
 *   - Supports both named and unnamed semaphores (this implementation uses
 *     unnamed).
 */
#pragma once

#include <semaphore.h>

#include <ctime>

#include "ISemaphore.hpp"

/**
 * @namespace PlatformCore
 * @brief Root namespace for platform abstraction interfaces and utilities.
 */
namespace PlatformCore {

/**
 * @brief POSIX-based counting semaphore implementation using POSIX semaphore
 * primitives.
 *
 * This class provides a concrete implementation of the ISemaphore interface
 * using POSIX sem_t functionality. It supports both blocking and timed
 * semaphore operations through sem_wait, sem_timedwait, and sem_post functions.
 *
 * The semaphore is initialized during the do_init() call using sem_init with
 * the initial count specified in the constructor. Supports absolute timeout
 * values for timed wait operations.
 *
 * @note This implementation is suitable for POSIX-compliant systems.
 * @note Uses unnamed semaphores (pshared=0) for thread synchronization within a
 * process.
 * @see ISemaphore for the base interface requirements
 */
class Semaphore : public ISemaphore<Semaphore, const struct timespec*> {
   public:
    using ISemaphore<Semaphore, const struct timespec*>::ISemaphore;

    /**
     * @brief Destroys the POSIX semaphore and releases resources.
     *
     * Automatically calls sem_destroy when the semaphore has been
     * successfully initialized. This avoids undefined behavior from
     * destroying an uninitialized semaphore while still guaranteeing that
     * system resources are reclaimed.
     */
    ~Semaphore() {
        if (is_initialized()) {
            sem_destroy(&sem);
        }
    }

    /**
     * @brief Initializes the POSIX semaphore using sem_init.
     *
     * Creates an unnamed semaphore for use within the current process
     * (pshared=0) with the initial count value specified in the constructor.
     * Must be called before any take() or give() operations.
     *
     * @return ErrorHandler with success (std::monostate) or SemaphoreError
     */
    auto do_init() -> ErrorHandler<std::monostate, SemaphoreError>;

    /**
     * @brief Attempts to take (decrement) the semaphore with optional timeout.
     *
     * This method implements the CRTP interface requirement for do_take(). It
     * supports two modes of operation:
     * - If abs_timeout is nullptr: Blocks indefinitely using sem_wait until
     * semaphore is available
     * - If abs_timeout is provided: Waits until the specified absolute time
     * using sem_timedwait
     *
     * The absolute timeout is based on CLOCK_REALTIME and specifies the
     * absolute time point when the wait should expire.
     *
     * @param abs_timeout Pointer to absolute time specification
     * (CLOCK_REALTIME) for timeout, or nullptr for indefinite blocking
     * @return ErrorHandler with success (std::monostate) or SemaphoreError
     */
    auto do_take(const struct timespec* abs_timeout)
        -> ErrorHandler<std::monostate, SemaphoreError>;

    /**
     * @brief Gives (increments) the semaphore using sem_post.
     *
     * Increments the semaphore count and wakes one waiting thread if any are
     * blocked on sem_wait or sem_timedwait. The operation never blocks.
     *
     * @note If the semaphore value would exceed SEM_VALUE_MAX, sem_post
     * behavior is platform-specific (typically fails with EOVERFLOW)
     */
    void do_give();

   private:
    sem_t sem{};  ///< Underlying POSIX semaphore handle
};

static_assert(SemaphoreImplementation<Semaphore, const struct timespec*>,
              "POSIX Semaphore must implement the required interface");

}  // namespace PlatformCore