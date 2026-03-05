#include "Led.hpp"

#include <logging/Logger.hpp>

#include "Gpio.hpp"
#include "Mutex.hpp"

namespace Boards::Led {

constexpr Boards::Gpio::Pin ledPin{
    .port = 1, /**< GPIO port identifier. */
    .pin = 0,  /**< GPIO pin number within the port. */
};

constexpr Boards::Gpio::GpioDriver gpioDriver{}; /**< GPIO driver instance. */
constinit PlatformCore::Mutex mutex{
    "LED"}; /**< Mutex for thread-safe LED access. */

auto LedBlinker::init() const -> bool {
    const auto mutex_result{mutex.init()};
    if (!mutex_result) {
        LOGGER_ERROR("Failed to initialize LED mutex, error: %d",
                     static_cast<int>(mutex_result.error()));
        return false;
    }

    const auto result{
        gpioDriver.set_mode(ledPin, Boards::Gpio::GpioMode::Output)};
    if (!result) {
        LOGGER_ERROR("Failed to set LED GPIO mode, error: %d",
                     static_cast<int>(result.error()));
        return false;
    }

    return true;
}

void LedBlinker::blink() {
    PlatformCore::MutexLocker lock(mutex, nullptr);
    if (lock) {
        status = !status;
        Boards::Gpio::DigitalState state{};

        if (status) {
            state = Boards::Gpio::DigitalState::Low;
        } else {
            state = Boards::Gpio::DigitalState::High;
        }
        gpioDriver.write(ledPin, state);
    }
}
}  // namespace Boards::Led