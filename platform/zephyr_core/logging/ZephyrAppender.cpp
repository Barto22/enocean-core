/**
 * @file ZephyrAppender.cpp
 * @brief plog-to-Zephyr logging bridge implementation.
 *
 * This translation unit owns the Zephyr log module registration so that all
 * plog output appears under a single "app" module in the Zephyr shell/RTT.
 * The write() method is called by plog from any thread; Zephyr's logging
 * subsystem is itself thread-safe.
 */
#include "ZephyrAppender.hpp"

#include <zephyr/logging/log.h>

#include <array>
#include <cstdio>
#include <string_view>

namespace {
// Register the Zephyr log module used for all application log output.
// NOLINTNEXTLINE(clang-diagnostic-reserved-identifier)
LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);
}  // namespace

namespace Logging {

void ZephyrAppender::write(const plog::Record& record) noexcept {
    const char* const func{record.getFunc()};
    const char* const msg{record.getMessage()};

    // Format: "function_name: message" — matches POSIX plog
    // TxtFormatter style.
    std::array<char, 512> buf{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    const auto written{
        std::snprintf(buf.data(), buf.size(), "%s: %s", func, msg)};
    if (written < 0 || static_cast<std::size_t>(written) >= buf.size()) {
        constexpr std::string_view k_truncated{"<truncated>"};
        buf.fill('\0');
        k_truncated.copy(buf.data(), k_truncated.size());
    }

    switch (record.getSeverity()) {
        case plog::fatal:
        case plog::error:
            // NOLINTNEXTLINE(clang-diagnostic-implicit-int-conversion,misc-redundant-expression)
            LOG_ERR("%s", buf.data());
            break;
        case plog::warning:
            // NOLINTNEXTLINE(clang-diagnostic-implicit-int-conversion,misc-redundant-expression)
            LOG_WRN("%s", buf.data());
            break;
        case plog::info:
            // NOLINTNEXTLINE(clang-diagnostic-implicit-int-conversion,misc-redundant-expression)
            LOG_INF("%s", buf.data());
            break;
        case plog::debug:
        case plog::verbose:
            // NOLINTNEXTLINE(clang-diagnostic-implicit-int-conversion,misc-redundant-expression)
            LOG_DBG("%s", buf.data());
            break;
        default:
            // NOLINTNEXTLINE(clang-diagnostic-implicit-int-conversion,misc-redundant-expression)
            LOG_INF("%s", buf.data());
            break;
    }
}

}  // namespace Logging
