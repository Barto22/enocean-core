#include "Gpio.hpp"

namespace Boards::Gpio {

namespace {
/**
 * @brief Helper function to convert Zephyr error codes to GpioError enum.
 */
constexpr auto errno_to_gpio_error(int error_code) -> GpioError {
    switch (-error_code) {
        case 0:
            return GpioError::Unknown;  // Should not happen
        case EINVAL:
            return GpioError::InvalidParameter;
        case ENODEV:
            return GpioError::DeviceNotAvailable;
        case EIO:
            return GpioError::IOError;
        case EBUSY:
            return GpioError::Busy;
        case ENOTSUP:
            return GpioError::NotSupported;
        default:
            return GpioError::Unknown;
    }
}
}  // namespace

auto GpioDriver::do_set_mode(const Pin& pin, GpioMode mode) const
    -> ErrorHandler<std::monostate, GpioError> {
    if (!gpio_is_ready_dt(pin.spec)) {
        return ErrorHandler<std::monostate, GpioError>(
            GpioError::DeviceNotAvailable, "GPIO device not ready");
    }

    std::uint32_t flags{0U};
    using enum GpioMode;
    switch (mode) {
        case Input:
            flags = GPIO_INPUT;
            break;
        case Output:
            flags = GPIO_OUTPUT;
            break;
        case InputPullup:
            flags = GPIO_INPUT | GPIO_PULL_UP;
            break;
        case InputPulldown:
            flags = GPIO_INPUT | GPIO_PULL_DOWN;
            break;
        case OutputPullup:
            flags = GPIO_OUTPUT | GPIO_PULL_UP;
            break;
        case OutputPulldown:
            flags = GPIO_OUTPUT | GPIO_PULL_DOWN;
            break;
        default:
            return ErrorHandler<std::monostate, GpioError>(
                GpioError::InvalidMode, "Invalid GPIO mode");
    }

    const auto ret{gpio_pin_configure_dt(pin.spec, flags)};
    if (ret < 0) {
        return ErrorHandler<std::monostate, GpioError>(
            errno_to_gpio_error(ret), "gpio_pin_configure_dt failed");
    }

    return ErrorHandler<std::monostate, GpioError>(std::monostate{});
}

void GpioDriver::do_deinit([[maybe_unused]] const Pin& pin) const {
    // Nothing to do for Zephyr
}

auto GpioDriver::do_read(const Pin& pin) const -> DigitalState {
    if (!gpio_is_ready_dt(pin.spec)) {
        return DigitalState::Low;
    }

    const int value{gpio_pin_get_dt(pin.spec)};
    using enum DigitalState;
    if (value < 0) {
        return Low;
    }

    return (value == 0) ? Low : High;
}

void GpioDriver::do_write(const Pin& pin, DigitalState state) const {
    if (!gpio_is_ready_dt(pin.spec)) {
        return;
    }

    const int value{(state == DigitalState::High) ? 1 : 0};
    const int ret{gpio_pin_set_dt(pin.spec, value)};
    if (ret < 0) {
        return;
    }
}

}  // namespace Boards::Gpio