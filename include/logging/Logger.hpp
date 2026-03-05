/**
 * @file Logger.hpp
 * @brief Platform-agnostic logging bridge to plog.
 *
 * Provides printf-style LOGGER_* macros that delegate to the plog backend.
 * All call sites use the same macros regardless of platform; platform-specific
 * plog initialisation (ConsoleAppender on POSIX, ZephyrAppender on Zephyr)
 * is performed once in each platform's main().
 *
 * Level mapping:
 *   LOGGER_NOTICE   -> plog::info
 *   LOGGER_WARNING  -> plog::warning
 *   LOGGER_ERROR    -> plog::error
 *   LOGGER_CRITICAL -> plog::fatal
 *
 * Implementation detail — source location:
 *   LogDispatch captures std::source_location::current() as a constructor
 *   default argument.  In C++20, default arguments are evaluated at the call
 *   site, so constructing LogDispatch{severity} inside a macro expansion
 *   records the file/line of the macro invocation, not of this header.
 *   This replaces the former ENOCEAN_LOG_IMPL helper macro entirely.
 */
#pragma once

#include <plog/Logger.h>
#include <plog/Record.h>

#include <array>
#include <cstdio>
#include <source_location>

namespace logging::detail {

/// Maximum formatted message length (matches previous logger buffer size).
inline constexpr std::size_t k_log_buf_size{256U};

/**
 * @brief Captures call-site source location and dispatches to plog.
 *
 * Constructing this struct in a macro causes std::source_location::current()
 * to be evaluated at the macro expansion point (the actual call site), giving
 * plog accurate file/line/function information.
 *
 * operator() formats the printf-style message into a fixed-size stack buffer
 * and submits a plog::Record with the stored source location.
 */
struct LogDispatch {
    plog::Severity severity{};  ///< plog severity level for this dispatch.
    std::source_location
        loc{};  ///< Call-site source location captured at construction.

    /// @brief Construct with severity and call-site location.
    /// @param sev  plog severity level.
    /// @param l    Source location, defaulting to the macro expansion point.
    consteval explicit LogDispatch(
        plog::Severity sev,
        std::source_location l = std::source_location::current()) noexcept
        : severity{sev}, loc{l} {}

    /// @brief Format and submit a printf-style log message to plog.
    /// @param fmt   printf-style format string.
    /// @param args  Format arguments.
    // Args passed by value: all printf arguments are POD/primitive types
    // and value semantics are correct for C variadic functions.
    template <typename... Args>
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    void operator()(const char* fmt, Args... args) const noexcept {
        auto* const logger{plog::get<PLOG_DEFAULT_INSTANCE_ID>()};
        if (logger == nullptr) {
            return;
        }
        if (!logger->checkSeverity(severity)) {
            return;
        }
        std::array<char, k_log_buf_size> buf{};
        // When there are no format arguments the format string is passed
        // through verbatim via "%s" to satisfy -Wformat-security, which only
        // fires when a non-literal format string has no accompanying arguments.
        if constexpr (sizeof...(args) == 0) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
            std::snprintf(buf.data(), buf.size(), "%s", fmt);
        } else {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, clang-diagnostic-format-nonliteral)
            std::snprintf(buf.data(), buf.size(), fmt, args...);
        }
        *logger +=
            plog::Record(severity, loc.function_name(),
                         static_cast<std::size_t>(loc.line()), loc.file_name(),
                         nullptr, PLOG_DEFAULT_INSTANCE_ID)
                .ref()
            << buf.data();
    }
};

}  // namespace logging::detail

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
/// Log an informational (notice-level) message.
#define LOGGER_NOTICE(fmt, ...) \
    ::logging::detail::LogDispatch{plog::info}(fmt, ##__VA_ARGS__)

/// Log a warning message.
#define LOGGER_WARNING(fmt, ...) \
    ::logging::detail::LogDispatch{plog::warning}(fmt, ##__VA_ARGS__)

/// Log an error message.
#define LOGGER_ERROR(fmt, ...) \
    ::logging::detail::LogDispatch{plog::error}(fmt, ##__VA_ARGS__)

/// Log a fatal/critical message.
#define LOGGER_CRITICAL(fmt, ...) \
    ::logging::detail::LogDispatch{plog::fatal}(fmt, ##__VA_ARGS__)
// NOLINTEND(cppcoreguidelines-macro-usage)
