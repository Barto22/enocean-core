
/**
 * @file Gpio.hpp
 * @brief Zephyr GPIO driver implementation for digital I/O abstraction.
 *
 * This file defines the Zephyr-specific GPIO driver class (`GpioDriver`)
 * implementing the generic GPIO interface (`IGpio`).
 *
 * - `GpioDriver` is a concrete implementation of `IGpio` for Zephyr platforms.
 * - `Pin` struct represents a GPIO pin (pointer to gpio_dt_spec).
 * - Compile-time check ensures correct implementation of the concept.
 *
 * Usage:
 *   - Use `GpioDriver` for digital I/O operations in Zephyr-based applications.
 *   - Pass a `Pin` struct to driver methods for pin control.
 */
#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <cstdint>

#include "GpioError.hpp"
#include "IGpio.hpp"

namespace Boards::Gpio {

/**
 * @struct Pin
 * @brief Zephyr GPIO pin identifier (pointer to gpio_dt_spec).
 */
struct Pin {
    const struct gpio_dt_spec*
        spec; /**< Pointer to Zephyr GPIO device tree spec. */
};

/**
 * @class GpioDriver
 * @brief Zephyr-specific GPIO driver implementation.
 *
 * Implements the `IGpio` interface for Zephyr platforms, providing digital I/O
 * control via Zephyr device tree and driver APIs.
 */
class GpioDriver final : public IGpio<GpioDriver, Pin> {
   public:
    using IGpio<GpioDriver, Pin>::IGpio;
    /**
     * @brief Set the mode of a GPIO pin.
     * @param pin Pin identifier.
     * @param mode Desired pin mode.
     * @return ErrorHandler with success (std::monostate) or GpioError
     */
    auto do_set_mode(const Pin& pin, GpioMode mode) const
        -> ErrorHandler<std::monostate, GpioError>;

    /**
     * @brief Deinitialize a GPIO pin.
     * @param pin Pin identifier.
     */
    void do_deinit(const Pin& pin) const;

    /**
     * @brief Read the digital state of a GPIO pin.
     * @param pin Pin identifier.
     * @return Digital state (low/high).
     */
    auto do_read(const Pin& pin) const -> DigitalState;

    /**
     * @brief Write a digital state to a GPIO pin.
     * @param pin Pin identifier.
     * @param state Digital state to write.
     */
    void do_write(const Pin& pin, DigitalState state) const;
};

/**
 * @brief Compile-time check for valid GPIO driver implementation.
 */
static_assert(GpioPinImplementation<GpioDriver, Pin>,
              "ZephyrGpioDriver not implemented correctly.");

}  // namespace Boards::Gpio