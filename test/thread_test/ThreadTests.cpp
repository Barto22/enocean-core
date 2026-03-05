#include <gtest/gtest.h>

#include <atomic>

#include "Thread.hpp"
#include "ThreadError.hpp"

namespace {
struct ThreadContext {
    std::atomic<int> runs{0};
};

void* TestThreadEntry(void* arg) {
    auto* ctx{static_cast<ThreadContext*>(arg)};
    if (ctx != nullptr) {
        ctx->runs.fetch_add(1, std::memory_order_relaxed);
    }
    return arg;
}

constexpr PlatformCore::Thread::ThreadStackInfo kDefaultStack{
    .stack_size = 4096,
    .stack_pointer = nullptr,
};
}  // namespace

TEST(PosixThreadTests, RunsAndDestroysCleanly) {
    ThreadContext ctx{};
    PlatformCore::Thread worker("worker");
    PlatformCore::ThreadParamVariant params{PlatformCore::ThreadParam1{
        .param = PlatformCore::ThreadParamFromPointer(&ctx),
    }};
    PlatformCore::ThreadFunctionVariant func{
        PlatformCore::ThreadFunction1(&TestThreadEntry)};

    auto create_result{worker.create(func, kDefaultStack, 0, params)};
    ASSERT_TRUE(create_result.has_value());
    auto destroy_result{worker.destroy()};
    ASSERT_TRUE(destroy_result.has_value());
    EXPECT_EQ(1, ctx.runs.load());
}

TEST(PosixThreadTests, SecondCreateFailsWhileActive) {
    ThreadContext ctx{};
    PlatformCore::Thread worker("worker");
    PlatformCore::ThreadParamVariant params{PlatformCore::ThreadParam1{
        .param = PlatformCore::ThreadParamFromPointer(&ctx),
    }};
    PlatformCore::ThreadFunctionVariant func{
        PlatformCore::ThreadFunction1(&TestThreadEntry)};

    auto create_result{worker.create(func, kDefaultStack, 0, params)};
    ASSERT_TRUE(create_result.has_value());

    PlatformCore::ThreadParamVariant second_params{params};
    auto second_create{worker.create(func, kDefaultStack, 0, second_params)};
    EXPECT_FALSE(second_create.has_value());
    EXPECT_EQ(PlatformCore::ThreadError::AlreadyCreated, second_create.error());

    auto destroy_result{worker.destroy()};
    ASSERT_TRUE(destroy_result.has_value());
    EXPECT_EQ(1, ctx.runs.load());
}
