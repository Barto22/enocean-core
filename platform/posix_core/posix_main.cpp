#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include <iostream>
#include <logging/Logger.hpp>

#include "AppEntry.hpp"
#include "Thread.hpp"
#include "build_info.hpp"

namespace {
constinit PlatformCore::Thread mainThread("MainApp");
}  // namespace

extern "C" auto main_thread([[maybe_unused]] void* thread_input) -> void* {
    App::AppEntry appEntry{};
    PlatformCore::ThreadParamVariant params{};
    appEntry.run(params);
    return nullptr;
}

// NOLINTBEGIN(bugprone-exception-escape)
auto main() noexcept -> int {
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender{};
    plog::init(plog::info, &consoleAppender);

    LOGGER_NOTICE("Build Time: %s", build_info::build_time);
    LOGGER_NOTICE("Git hash: %s", build_info::git_commit);

    constexpr PlatformCore::Thread::ThreadStackInfo mainStackInfo{
        .stack_size = 4096,
        .stack_pointer = nullptr,
    };
    const auto th_result{mainThread.create(main_thread, mainStackInfo, 1)};
    if (!th_result) {
        std::cerr << "Failed to create main thread, error: "
                  << static_cast<int>(th_result.error()) << "\n";
        return -1;
    }

    // Block until the application thread exits.
    (void)mainThread.destroy();
    return 0;
}
// NOLINTEND(bugprone-exception-escape)
