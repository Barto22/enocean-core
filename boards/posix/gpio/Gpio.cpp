#include "Gpio.hpp"

#include <logging/Logger.hpp>
#include <utility>

namespace Boards::Gpio {

auto GpioDriver::do_set_mode(const Pin& pin, GpioMode mode) const
    -> ErrorHandler<std::monostate, GpioError> {
    LOGGER_NOTICE("Setting POSIX pin %d mode to %d", pin.pin,
                  std::to_underlying<GpioMode>(mode));
    return ErrorHandler<std::monostate, GpioError>(std::monostate{});
}

void GpioDriver::do_deinit(const Pin& pin) const {
    LOGGER_NOTICE("Deinitializing POSIX pin %d", pin.pin);
}

auto GpioDriver::do_read(const Pin& pin) const -> DigitalState {
    LOGGER_NOTICE("Read POSIX pin %d", pin.pin);
    return DigitalState::Low;  // Default return value
}

void GpioDriver::do_write(const Pin& pin, DigitalState state) const {
    LOGGER_NOTICE("Writing %d to POSIX pin %d",
                  std::to_underlying<DigitalState>(state), pin.pin);
}

}  // namespace Boards::Gpio