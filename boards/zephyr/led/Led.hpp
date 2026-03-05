
/**
 * @file Led.hpp
 * @brief Zephyr LED blinker abstraction for demonstration and testing.
 *
 * This file defines a simple LED blinker class (`LedBlinker`) for Zephyr
 * platforms.
 *
 * - `LedBlinker` provides basic LED initialization and blinking functionality.
 *
 * Usage:
 *   - Use `LedBlinker` for demonstration or testing of digital output.
 *   - Call `init()` to initialize, then `blink()` to toggle the LED state.
 */
#pragma once

/**
 * @namespace Boards::Led
 * @brief Namespace for Zephyr LED abstractions.
 */
namespace Boards::Led {

/**
 * @class LedBlinker
 * @brief Simple LED blinker abstraction for Zephyr platforms.
 *
 * Provides basic methods for LED initialization and blinking.
 */
class LedBlinker {
   public:
    /**
     * @brief Construct a new LedBlinker object (initially off).
     */
    LedBlinker() noexcept = default;
    /**
     * @brief Initialize the LED hardware or abstraction.
     * @return True if initialization succeeded, false otherwise.
     */
    auto init() const -> bool;
    /**
     * @brief Toggle or blink the LED.
     */
    void blink();

   private:
    /** @brief Current LED status (on/off). */
    bool status{false};
};
}  // namespace Boards::Led