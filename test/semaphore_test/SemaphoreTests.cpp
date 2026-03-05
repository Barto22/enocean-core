#include <gtest/gtest.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <thread>

#include "Semaphore.hpp"
#include "SemaphoreError.hpp"

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

TEST(PosixSemaphoreTests, InitTakeGive) {
    PlatformCore::Semaphore sem("basic", 1, 1);
    auto init_result{sem.init()};
    ASSERT_TRUE(init_result.has_value());

    auto take_result{sem.take(nullptr)};
    EXPECT_TRUE(take_result.has_value());
    sem.give();
}

TEST(PosixSemaphoreTests, InitialCountZeroBlocksUntilGive) {
    PlatformCore::Semaphore sem("zero_init", 0, 1);
    auto init_result{sem.init()};
    ASSERT_TRUE(init_result.has_value());

    std::atomic<bool> take_succeeded{false};
    std::atomic<bool> thread_started{false};

    std::thread waiter([&]() {
        thread_started.store(true, std::memory_order_release);
        auto result{sem.take(nullptr)};
        take_succeeded.store(result.has_value(), std::memory_order_relaxed);
    });

    // Wait for thread to start and block
    while (!thread_started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    // Semaphore should not have been acquired yet
    EXPECT_FALSE(take_succeeded.load());

    // Now give the semaphore
    sem.give();

    waiter.join();
    EXPECT_TRUE(take_succeeded.load());
}

TEST(PosixSemaphoreTests, TimedTakeTimesOutWhenUnavailable) {
    PlatformCore::Semaphore sem("timeout", 0, 1);
    auto init_result{sem.init()};
    ASSERT_TRUE(init_result.has_value());

    const auto deadline{MakeAbsoluteTimeout(std::chrono::milliseconds{50})};
    auto take_result{sem.take(&deadline)};

    EXPECT_FALSE(take_result.has_value());
    if (!take_result.has_value()) {
        EXPECT_EQ(take_result.error(), PlatformCore::SemaphoreError::Timeout);
    }
}

TEST(PosixSemaphoreTests, CountingSemaphoreBehavior) {
    constexpr unsigned int max_count{3};
    PlatformCore::Semaphore sem("counting", max_count, max_count);
    auto init_result{sem.init()};
    ASSERT_TRUE(init_result.has_value());

    // Should be able to take max_count times
    for (unsigned int i = 0; i < max_count; ++i) {
        auto take_result{sem.take(nullptr)};
        EXPECT_TRUE(take_result.has_value())
            << "Failed to take semaphore at iteration " << i;
    }

    // Next take should timeout since count is now zero
    const auto deadline{MakeAbsoluteTimeout(std::chrono::milliseconds{50})};
    auto timeout_take{sem.take(&deadline)};
    EXPECT_FALSE(timeout_take.has_value());

    // Give back and we should be able to take again
    sem.give();
    auto final_take{sem.take(nullptr)};
    EXPECT_TRUE(final_take.has_value());
}

TEST(PosixSemaphoreTests, MultipleThreadsWaitingForSemaphore) {
    constexpr int num_threads{3};
    PlatformCore::Semaphore sem("multi", 0, num_threads);
    auto init_result{sem.init()};
    ASSERT_TRUE(init_result.has_value());

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            auto result{sem.take(nullptr)};
            if (result.has_value()) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Wait for all threads to start and block
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Release semaphore num_threads times
    for (int i = 0; i < num_threads; ++i) {
        sem.give();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads);
}

TEST(PosixSemaphoreTests, TakeWithoutInitFails) {
    PlatformCore::Semaphore sem("uninit", 1, 1);
    // Not calling init()

    auto take_result{sem.take(nullptr)};
    EXPECT_FALSE(take_result.has_value());
    if (!take_result.has_value()) {
        EXPECT_EQ(take_result.error(),
                  PlatformCore::SemaphoreError::NotInitialized);
    }
}

TEST(PosixSemaphoreTests, DoubleInitReinitializes) {
    PlatformCore::Semaphore sem("reinit", 1, 1);

    const auto first_init{sem.init()};
    ASSERT_TRUE(first_init.has_value());

    // Second init should return already initialized
    const auto second_init{sem.init()};
    EXPECT_FALSE(second_init.has_value());
    if (!second_init.has_value()) {
        EXPECT_EQ(second_init.error(),
                  PlatformCore::SemaphoreError::AlreadyInitialized);
    }

    // Semaphore should still be usable
    const auto take_result{sem.take(nullptr)};
    EXPECT_TRUE(take_result.has_value());
}
