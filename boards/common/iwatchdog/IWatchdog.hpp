/**
 * @file IWatchdog.hpp
 * @brief Platform-agnostic watchdog timer interface for hardware monitoring.
 *
 * This file defines the base watchdog timer interface (`IWatchdog`) using the
 * CRTP pattern for compile-time polymorphism. The interface provides a modern
 * C++ abstraction for hardware watchdog functionality across different
 * platforms (Zephyr, STM32, etc.).
 *
 * - `IWatchdog` is a CRTP base class for watchdog timer implementations.
 * - `WatchdogImplementation` concept validates conforming implementations.
 *
 * Usage:
 *   - Inherit from `IWatchdog<YourClass>` and implement init_impl() and
 *     feed_impl() for your platform.
 *   - Use the interface for portable watchdog operations in your application.
 */
#pragma once

#include <concepts>
#include <cstdint>

#include "WatchdogError.hpp"

namespace Boards::Watchdog {

/**
 * @brief CRTP base interface for hardware watchdog timer implementations.
 *
 * This class provides a platform-agnostic interface for watchdog timer
 * functionality using the Curiously Recurring Template Pattern (CRTP).
 * Derived classes must implement init_impl() and reload_impl() methods
 * to provide platform-specific watchdog behavior.
 *
 * @tparam Derived The derived class type implementing the watchdog interface.
 */
template <typename Derived>
class IWatchdog {
   public:
    /**
     * @brief Deleted default constructor.
     */
    consteval IWatchdog() noexcept = default;

    /**
     * @brief Deleted copy constructor.
     */
    IWatchdog(const IWatchdog&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    IWatchdog& operator=(const IWatchdog&) = delete;

    /**
     * @brief Deleted move constructor.
     */
    IWatchdog(IWatchdog&&) = delete;

    /**
     * @brief Deleted move assignment operator.
     */
    IWatchdog& operator=(IWatchdog&&) = delete;

    /**
     * @brief Initialize the watchdog timer with the specified timeout period.
     *
     * Configures and starts the hardware watchdog timer. The watchdog must be
     * periodically reloaded (via feed()) before the timeout expires to
     * prevent a system reset.
     *
     * @param timeout Watchdog timeout period in milliseconds. The watchdog will
     *                trigger a reset if not reloaded within this time.
     * @return ErrorHandler with std::monostate on success, WatchdogError on
     * failure
     */
    [[nodiscard]] auto init(const std::uint32_t timeout) const
        -> ErrorHandler<std::monostate, WatchdogError> {
        return static_cast<const Derived*>(this)->init_impl(timeout);
    }

    /**
     * @brief Feed (kick/refresh) the watchdog timer to prevent system reset.
     *
     * Resets the watchdog counter to prevent timeout. This method must be
     * called periodically before the timeout period expires to keep the system
     * running. Also known as "kicking" or "feeding" the watchdog.
     */
    void feed() const { static_cast<const Derived*>(this)->feed_impl(); }

   protected:
    /**
     * @brief Protected destructor to prevent deletion through base class
     * pointer.
     */
    ~IWatchdog() noexcept = default;
};

/**
 * @brief Concept to validate watchdog implementation requirements.
 *
 * Ensures that a type T provides the required interface methods for watchdog
 * functionality: init_impl() returning bool and feed_impl() returning void.
 * The type must also be a concrete (non-abstract) class.
 *
 * @tparam T The type to check for watchdog implementation compliance.
 */
template <typename T>
concept WatchdogImplementation =
    requires(const T& watchdog, const std::uint32_t timeout) {
        {
            watchdog.init_impl(timeout)
        } -> std::same_as<ErrorHandler<std::monostate, WatchdogError>>;
        { watchdog.feed_impl() } -> std::same_as<void>;
        requires !std::is_abstract_v<T>;
    };

}  // namespace Boards::Watchdog