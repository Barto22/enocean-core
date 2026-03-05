
/**
 * @file IGpio.hpp
 * @brief GPIO abstraction interface and concept for cross-platform hardware
 * drivers.
 *
 * This file defines the generic GPIO interface (`IGpio`) and concept for
 * validating platform-specific GPIO implementations. The interface provides a
 * modern C++ abstraction for digital I/O pin control with ErrorHandler-based
 * error handling, supporting multiple platforms and pin types.
 *
 * - `IGpio` is an abstract base class for GPIO drivers.
 * - `GpioPinImplementation` concept checks for valid GPIO implementations.
 *
 * Usage:
 *   - Implement `IGpio<PinType>` for your platform (e.g., STM32, POSIX,
 * Zephyr).
 *   - Use the interface for portable digital I/O operations in your
 * application.
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

#include "GpioError.hpp"

/**
 * @namespace Boards::Gpio
 * @brief Namespace for common GPIO abstractions and concepts.
 */
namespace Boards::Gpio {

/**
 * @enum GpioMode
 * @brief GPIO pin mode enumeration.
 */
enum class GpioMode : std::uint8_t {
    Input,
    Output,
    Analog,
    InputPullup,
    InputPulldown,
    OutputPullup,
    OutputPulldown,
    AltFunction
};

/**
 * @enum DigitalState
 * @brief Digital pin state (low/high).
 */
enum class DigitalState : std::uint8_t { Low = 0, High = 1 };

/**
 * @class IGpio
 * @brief Abstract base class for generic GPIO drivers.
 *
 * Provides a platform-agnostic interface for digital I/O pin control.
 *
 * @tparam Derived The derived class implementing the actual GPIO functionality.
 * @tparam PinType Type representing a pin (platform-specific).
 */
template <typename Derived, typename PinType>
/**
 * @brief Abstract base class for generic GPIO drivers.
 * @tparam PinType Type representing a pin (platform-specific).
 */
class IGpio {
   public:
    /**
     * @brief Default constructor.
     */
    consteval IGpio() noexcept = default;

    /**
     * @brief Deleted copy constructor.
     */
    IGpio(const IGpio&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    IGpio& operator=(const IGpio&) = delete;

    /**
     * @brief Default move constructor.
     */
    IGpio(IGpio&&) noexcept = default;

    /**
     * @brief Default move assignment operator.
     */
    IGpio& operator=(IGpio&&) noexcept = default;

    /**
     * @brief Set the mode of a GPIO pin.
     * @param pin Pin identifier.
     * @param mode Desired pin mode.
     * @return ErrorHandler with success (std::monostate) or GpioError
     */
    [[nodiscard]] auto set_mode(const PinType& pin, GpioMode mode) const
        -> ErrorHandler<std::monostate, GpioError> {
        return static_cast<const Derived*>(this)->do_set_mode(pin, mode);
    }

    /**
     * @brief Deinitialize a GPIO pin.
     * @param pin Pin identifier.
     */
    void deinit(const PinType& pin) const {
        static_cast<const Derived*>(this)->do_deinit(pin);
    }

    /**
     * @brief Read the digital state of a GPIO pin.
     * @param pin Pin identifier.
     * @return Digital state (low/high).
     */
    auto read(const PinType& pin) const -> DigitalState {
        return static_cast<const Derived*>(this)->do_read(pin);
    }

    /**
     * @brief Write a digital state to a GPIO pin.
     * @param pin Pin identifier.
     * @param state Digital state to write.
     */
    void write(const PinType& pin, DigitalState state) const {
        static_cast<const Derived*>(this)->do_write(pin, state);
    }

   protected:
    /**
     * @brief Virtual destructor for safe polymorphic deletion.
     */
    ~IGpio() noexcept = default;
};

/**
 * @brief Concept for valid GPIO driver implementations.
 * @tparam T GPIO driver type.
 * @tparam PinType Pin identifier type.
 */
template <typename T, typename PinType>
concept GpioPinImplementation =
    requires(T gpio, PinType pin, GpioMode mode, DigitalState state) {
        {
            gpio.do_set_mode(pin, mode)
        } -> std::same_as<ErrorHandler<std::monostate, GpioError>>;
        { gpio.do_deinit(pin) } -> std::same_as<void>;
        { gpio.do_read(pin) } -> std::same_as<DigitalState>;
        { gpio.do_write(pin, state) } -> std::same_as<void>;
        requires !std::is_abstract_v<T>;
    };

}  // namespace Boards::Gpio