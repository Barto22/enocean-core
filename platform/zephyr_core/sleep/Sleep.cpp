#include "Sleep.hpp"

// NOLINTNEXTLINE(misc-header-include-cycle)
#include <zephyr/kernel.h>

namespace PlatformCore {
void Sleep::do_sleep_ms(std::uint32_t ms_sleep) const {
    k_msleep(static_cast<std::int32_t>(ms_sleep));
}
}  // namespace PlatformCore