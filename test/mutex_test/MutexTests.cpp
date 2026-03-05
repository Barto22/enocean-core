#include <gtest/gtest.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <thread>

#include "Mutex.hpp"
#include "MutexError.hpp"

namespace {
auto MakeAbsoluteTimeout(std::chrono::milliseconds delta) -> timespec {
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);

    const auto delta_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(delta)};
    const auto seconds{
        std::chrono::duration_cast<std::chrono::seconds>(delta_ns)};
    ts.tv_sec += static_cast<time_t>(seconds.count());

    const auto extra_ns{static_cast<int64_t>(delta_ns.count() -
                                             seconds.count() * 1000000000LL)};
    ts.tv_nsec += extra_ns;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return ts;
}
}  // namespace

TEST(PosixMutexTests, InitLockUnlock) {
    PlatformCore::Mutex mutex{"basic"};
    auto init_result{mutex.init()};
    ASSERT_TRUE(init_result.has_value());

    auto lock_result{mutex.lock(nullptr)};
    EXPECT_TRUE(lock_result.has_value());
    mutex.unlock();
}

TEST(PosixMutexTests, TimedLockTimesOutWhenAlreadyLocked) {
    PlatformCore::Mutex mutex{"timeout"};
    auto init_result{mutex.init()};
    ASSERT_TRUE(init_result.has_value());

    auto first_lock{mutex.lock(nullptr)};
    ASSERT_TRUE(first_lock.has_value());

    std::atomic<bool> lock_succeeded{false};
    std::atomic<bool> is_timeout{false};
    std::thread waiter([&]() {
        const auto deadline{MakeAbsoluteTimeout(std::chrono::milliseconds{50})};
        auto result{mutex.lock(&deadline)};
        lock_succeeded.store(result.has_value(), std::memory_order_relaxed);
        if (!result.has_value()) {
            is_timeout.store(
                result.error() == PlatformCore::MutexError::Timeout,
                std::memory_order_relaxed);
        }
    });

    waiter.join();
    EXPECT_FALSE(lock_succeeded.load());
    EXPECT_TRUE(is_timeout.load());
    mutex.unlock();
}

TEST(PosixMutexTests, DoubleInitReinitializes) {
    PlatformCore::Mutex mutex("reinit");

    const auto first_init{mutex.init()};
    ASSERT_TRUE(first_init.has_value());

    // Second init should return already initialized
    const auto second_init{mutex.init()};
    EXPECT_FALSE(second_init.has_value());
    if (!second_init.has_value()) {
        EXPECT_EQ(second_init.error(),
                  PlatformCore::MutexError::AlreadyInitialized);
    }

    // Mutex should still be usable
    const auto lock_result{mutex.lock(nullptr)};
    EXPECT_TRUE(lock_result.has_value());
    mutex.unlock();
}
