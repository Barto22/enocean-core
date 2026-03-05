
/**
 * @file Sleep.hpp
 * @brief Zephyr sleep implementation for platform timing abstraction.
 *
 * This file defines the Zephyr-specific sleep class (`Sleep`) implementing the
 * core sleep/timing interface (`ISleep`).
 *
 * - `Sleep` is a concrete implementation of `ISleep` for Zephyr platforms.
 * - `SleepImplementation` concept checks for valid sleep implementations.
 *
 * Usage:
 *   - Use `Sleep` for thread suspension in Zephyr-based applications.
 *   - Pass a `Sleep` instance to platform code for portable sleep operations.
 */
#pragma once

#include "ISleep.hpp"

namespace PlatformCore {
/**
 * @class Sleep
 * @brief Zephyr-specific sleep implementation.
 *
 * Implements the `ISleep` interface for Zephyr platforms, providing thread
 * suspension via Zephyr mechanisms (e.g., k_sleep).
 */
class Sleep final : public ISleep<Sleep> {
   public:
    using ISleep<Sleep>::ISleep;

    /**
     * @brief Suspends the current Zephyr thread for the specified duration in
     * milliseconds.
     * @param ms_sleep The duration in milliseconds.
     */
    void do_sleep_ms(std::uint32_t ms_sleep) const;
};

/**
 * @brief Compile-time check for valid sleep implementation.
 */
static_assert(
    SleepImplementation<Sleep>,
    "The Zephyr Sleep must correctly implement the ISleep interface.");

}  // namespace PlatformCore