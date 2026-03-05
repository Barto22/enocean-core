/**
 * @file Watchdog.hpp
 * @brief Zephyr RTOS watchdog driver implementation.
 *
 * This file provides a concrete implementation of the IWatchdog interface
 * for the Zephyr RTOS platform. It wraps Zephyr's native watchdog API to
 * provide hardware watchdog timer functionality with a consistent interface.
 */
#pragma once

#include <cstdint>

#include "IWatchdog.hpp"

namespace Boards::Watchdog {

/**
 * @brief Zephyr RTOS watchdog timer driver implementation.
 *
 * Concrete implementation of the IWatchdog interface for Zephyr RTOS.
 * This driver provides hardware watchdog functionality using Zephyr's
 * native watchdog device API. The watchdog must be periodically fed
 * to prevent system resets.
 *
 * @note This class is final and cannot be further derived.
 */
class WatchdogDriver final : public IWatchdog<WatchdogDriver> {
   public:
    using IWatchdog<WatchdogDriver>::IWatchdog;

    /**
     * @brief Initialize the Zephyr watchdog timer with specified timeout.
     *
     * Configures and starts the hardware watchdog timer using Zephyr's
     * device API. Once initialized, the watchdog must be fed periodically
     * via feed_impl() before the timeout expires.
     *
     * @param timeout Watchdog timeout period in milliseconds.
     * @return ErrorHandler with std::monostate on success, WatchdogError on
     * failure
     */
    auto init_impl(const std::uint32_t timeout) const
        -> ErrorHandler<std::monostate, WatchdogError>;

    /**
     * @brief Feed the watchdog timer to prevent system reset.
     *
     * Resets the watchdog counter using Zephyr's watchdog feed API.
     * This must be called periodically before the configured timeout
     * period expires to keep the system running.
     */
    void feed_impl() const;

   private:
    int wdt_channel_id{};
};

/**
 * @brief Compile-time check for valid watchdog driver implementation.
 */
static_assert(WatchdogImplementation<WatchdogDriver>,
              "ZephyrWatchdogDriver not implemented correctly.");
}  // namespace Boards::Watchdog