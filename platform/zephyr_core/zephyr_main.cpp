#include <app_version.h>
#include <plog/Init.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>

#include <logging/Logger.hpp>

#include "AppEntry.hpp"
#include "Sleep.hpp"
#include "Thread.hpp"
#include "Watchdog.hpp"
#include "ZephyrAppender.hpp"
#include "build_info.hpp"

K_THREAD_STACK_DEFINE(main_stack, 4096);

namespace {
constexpr PlatformCore::Thread::ThreadStackInfo mainStackInfo{
    K_THREAD_STACK_SIZEOF(main_stack), main_stack};
constinit PlatformCore::Thread mainThread(static_cast<const char*>("MainApp"));
constinit Logging::ZephyrAppender zephyrAppender{};
}  // namespace

extern "C" void main_thread([[maybe_unused]] void* p1,
                            [[maybe_unused]] void* p2,
                            [[maybe_unused]] void* p3) {
    App::AppEntry appEntry{};
    PlatformCore::ThreadParamVariant params{};
    appEntry.run(params);
}

int main() {
    plog::init(plog::info, &zephyrAppender);

    Boards::Watchdog::WatchdogDriver watchdog{};

    auto watchdog_result{watchdog.init(10000)};
    if (watchdog_result) {
        LOGGER_NOTICE("Watchdog initialization passed.");
    } else {
        LOGGER_CRITICAL("Failed to initialize watchdog!");
        return -1;
    }

    LOGGER_NOTICE("Build Time: %s", build_info::build_time);
    LOGGER_NOTICE("Git hash: %s", build_info::git_commit);
    LOGGER_NOTICE("App Version: %s", APP_VERSION_EXTENDED_STRING);

    // Enable the Bluetooth stack (required before Ble::Scanner::init).
    const int bt_err{bt_enable(nullptr)};
    if (bt_err != 0) {
        LOGGER_ERROR("bt_enable failed (%d)", bt_err);
        return -1;
    }

    const auto th_result{mainThread.create(main_thread, mainStackInfo, 1)};
    if (!th_result) {
        LOGGER_ERROR("Failed to create main application thread, error: %d",
                     static_cast<int>(th_result.error()));
        return -1;
    }

    PlatformCore::Sleep sleep{};

    while (true) {
        watchdog.feed();
        sleep.sleep_ms(1000);
    }

    return 0;
}
