#include "Watchdog.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
// NOLINTNEXTLINE(misc-header-include-cycle)
#include <zephyr/kernel.h>

namespace Boards::Watchdog {

namespace {
#if DT_NODE_HAS_STATUS(DT_ALIAS(watchdog0), okay)
const struct device* const wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));
#else
#error "watchdog0 alias not defined or disabled in devicetree"
#endif

inline auto& get_wdt_channel_id() noexcept {
    static constinit int wdt_channel_id{-1};
    return wdt_channel_id;
}

/**
 * @brief Convert Zephyr errno to WatchdogError
 * @param err errno value
 * @return Corresponding WatchdogError
 */
constexpr auto errno_to_watchdog_error(int err) -> WatchdogError {
    using enum WatchdogError;
    switch (err) {
        case -ENOTSUP:
            return NotSupported;
        case -EINVAL:
            return InvalidParameter;
        case -ENODEV:
            return DeviceNotFound;
        case -EBUSY:
            return AlreadyInitialized;
        default:
            return HardwareError;
    }
}
}  // namespace

auto WatchdogDriver::init_impl(const std::uint32_t timeout) const
    -> ErrorHandler<std::monostate, WatchdogError> {
    if (!device_is_ready(wdt)) {
        return ErrorHandler<std::monostate, WatchdogError>(
            WatchdogError::DeviceNotReady, "Watchdog device not ready");
    }

    struct wdt_timeout_cfg wdt_config{};
    wdt_config.flags = WDT_FLAG_RESET_SOC;
    wdt_config.window.min = 0;
    wdt_config.window.max = timeout;

    get_wdt_channel_id() = wdt_install_timeout(wdt, &wdt_config);

    if (get_wdt_channel_id() == -ENOTSUP) {
        wdt_config.callback = nullptr;
        get_wdt_channel_id() = wdt_install_timeout(wdt, &wdt_config);
    }

    if (get_wdt_channel_id() < 0) {
        return ErrorHandler<std::monostate, WatchdogError>(
            WatchdogError::ConfigurationError,
            "Failed to install watchdog timeout");
    }

    const auto setup_result{wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG)};
    if (setup_result != 0) {
        return ErrorHandler<std::monostate, WatchdogError>(
            errno_to_watchdog_error(setup_result), "Failed to setup watchdog");
    }

    return ErrorHandler<std::monostate, WatchdogError>(std::monostate{});
}

void WatchdogDriver::feed_impl() const { wdt_feed(wdt, get_wdt_channel_id()); }
}  // namespace Boards::Watchdog