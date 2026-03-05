#include "Led.hpp"

#include "Gpio.hpp"
#include "Mutex.hpp"

namespace Boards::Led {

#define LED0_NODE DT_ALIAS(led0)

inline constexpr struct gpio_dt_spec led =
    GPIO_DT_SPEC_GET(LED0_NODE, gpios); /**< Zephyr GPIO spec for LED0 */
constexpr Boards::Gpio::Pin ledPin{
    .spec = &led,
};

constexpr Boards::Gpio::GpioDriver gpioDriver{};
constinit PlatformCore::Mutex mutex{static_cast<const char*>("LED")};

auto LedBlinker::init() const -> bool {
    const auto mutex_result{mutex.init()};
    if (!mutex_result) {
        return false;
    }

    const auto result{
        gpioDriver.set_mode(ledPin, Boards::Gpio::GpioMode::Output)};
    if (!result) {
        return false;
    }

    return true;
}

void LedBlinker::blink() {
    PlatformCore::MutexLocker lock(mutex, K_FOREVER);
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