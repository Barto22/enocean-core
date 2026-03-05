
/**
 * @file Sleep.hpp
 * @brief POSIX sleep implementation for platform timing abstraction.
 *
 * This file defines the POSIX-specific sleep class (`Sleep`) implementing the
 * core sleep/timing interface (`ISleep`).
 *
 * - `Sleep` is a concrete implementation of `ISleep` for POSIX platforms.
 * - `SleepImplementation` concept checks for valid sleep implementations.
 *
 * Usage:
 *   - Use `Sleep` for thread suspension in POSIX-based applications.
 *   - Pass a `Sleep` instance to platform code for portable sleep operations.
 */
#pragma once

#include <cstdint>

#include "ISleep.hpp"

/**
 * @namespace PlatformCore
 * @brief Root namespace for platform abstraction interfaces and utilities.
 */
namespace PlatformCore {
/**
 * @class Sleep
 * @brief POSIX-specific sleep implementation.
 *
 * Implements the `ISleep` interface for POSIX platforms, providing thread
 * suspension via POSIX mechanisms (e.g., nanosleep, usleep, etc).
 */
class Sleep final : public ISleep<Sleep> {
   public:
    using ISleep<Sleep>::ISleep;

    /**
     * @brief Suspends the current POSIX thread for the specified duration in
     * milliseconds.
     * @param ms_sleep The duration in milliseconds.
     */
    void do_sleep_ms(std::uint32_t ms_sleep) const;
};

/**
 * @brief Compile-time check for valid sleep implementation.
 */
static_assert(SleepImplementation<Sleep>,
              "The POSIX Sleep must correctly implement the ISleep interface.");

}  // namespace PlatformCore