
/**
 * @file ISleep.hpp
 * @brief Platform-agnostic timing/sleep interface for cross-platform projects.
 *
 * This file defines the base sleep/timing interface (`ISleep`) and a concept
 * for validating platform-specific implementations. The interface provides a
 * simple, modern C++ abstraction for thread sleep functionality using the
 * CRTP (Curiously Recurring Template Pattern).
 *
 * - `ISleep` is a CRTP base class for compile-time polymorphic sleep/timing
 * services.
 * - `SleepImplementation` concept validates conforming implementations at
 * compile time.
 *
 * Usage:
 *   - Inherit from `ISleep<YourClass>` and implement do_sleep_ms() for your
 * platform (e.g., POSIX, Zephyr).
 *   - Use the interface for portable sleep/timing operations in your
 * application.
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

/**
 * @namespace PlatformCore
 * @brief Root namespace for platform abstraction interfaces and utilities.
 */
namespace PlatformCore {
/**
 * @class ISleep
 * @brief CRTP-based interface for platform-agnostic timing/sleep services.
 *
 * Provides a compile-time polymorphic interface for suspending threads using
 * the Curiously Recurring Template Pattern (CRTP). Derived classes must
 * implement do_sleep_ms() to define platform-specific sleep behavior with
 * millisecond resolution.
 *
 * This design avoids runtime polymorphism overhead while providing a clean
 * abstraction for cross-platform sleep functionality without depending on
 * C++ Standard Library threading primitives.
 *
 * @tparam Derived The derived class implementing the actual sleep functionality
 */
template <typename Derived>
class ISleep {
   public:
    /**
     * @brief Default destructor.
     * @note Non-virtual as CRTP doesn't require runtime polymorphism.
     */
    ~ISleep() noexcept = default;

    /**
     * @brief Default constructor.
     */
    ISleep() noexcept = default;
    /**
     * @brief Deleted copy constructor.
     */
    ISleep(const ISleep&) = delete;
    /**
     * @brief Deleted copy assignment operator.
     */
    ISleep& operator=(const ISleep&) = delete;
    /**
     * @brief Deleted move constructor.
     */
    ISleep(ISleep&&) = delete;
    /**
     * @brief Deleted move assignment operator.
     */
    ISleep& operator=(ISleep&&) = delete;

    /**
     * @brief Suspends the current thread for the specified duration in
     * milliseconds.
     *
     * Delegates to the derived class's do_sleep_ms() implementation to perform
     * the actual sleep operation. This allows for platform-specific sleep
     * mechanisms while maintaining a uniform interface.
     *
     * @param ms_sleep The duration in milliseconds to sleep
     */
    void sleep_ms(std::uint32_t ms_sleep) const {
        static_cast<const Derived*>(this)->do_sleep_ms(ms_sleep);
    }
};

/**
 * @brief Concept defining the requirements for a valid sleep implementation.
 *
 * A type satisfies SleepImplementation if it provides:
 * - do_sleep_ms(std::uint32_t) method returning void
 * - Is a concrete (non-abstract) type
 *
 * @tparam T The type to check for sleep implementation compliance
 */
template <typename T>
concept SleepImplementation = requires(T sleep, std::uint32_t ms) {
    { sleep.do_sleep_ms(ms) } -> std::same_as<void>;
    requires !std::is_abstract_v<T>;
};

}  // namespace PlatformCore