
/**
 * @file Gpio.hpp
 * @brief POSIX GPIO driver implementation for digital I/O abstraction.
 *
 * This file defines the POSIX-specific GPIO driver class (`GpioDriver`)
 * implementing the generic GPIO interface (`IGpio`).
 *
 * - `GpioDriver` is a concrete implementation of `IGpio` for POSIX platforms.
 * - `Pin` struct represents a GPIO pin (port and pin number).
 * - Compile-time check ensures correct implementation of the concept.
 *
 * Usage:
 *   - Use `GpioDriver` for digital I/O operations in POSIX-based applications.
 *   - Pass a `Pin` struct to driver methods for pin control.
 */
#pragma once

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>

#include "GpioError.hpp"
#include "IGpio.hpp"

namespace Boards::Gpio {

/**
 * @struct Pin
 * @brief POSIX GPIO pin identifier (port and pin number).
 */
struct Pin {
    std::uint32_t port; /**< GPIO port identifier. */
    std::uint32_t pin;  /**< GPIO pin number within the port. */
};

/**
 * @class GpioDriver
 * @brief POSIX-specific GPIO driver implementation.
 *
 * Implements the `IGpio` interface for POSIX platforms, providing digital I/O
 * control via file descriptors and system calls.
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
              "PosixGpioDriver not implemented correctly.");

}  // namespace Boards::Gpio