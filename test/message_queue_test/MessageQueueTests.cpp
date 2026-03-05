#include <gtest/gtest.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <string>

#include "MessageQueue.hpp"
#include "MessageQueueError.hpp"

namespace {
struct TestMessage {
    std::uint32_t value{};
};

auto UniqueQueueName() -> std::string {
    static constinit std::atomic<int> counter{0};
    const pid_t pid{getpid()};
    const int id{counter.fetch_add(1, std::memory_order_relaxed)};
    return "/mq_" + std::to_string(pid) + "_" + std::to_string(id);
}
}  // namespace

TEST(PosixMessageQueueTests, InitFailsWithoutLeadingSlash) {
    PlatformCore::MessageQueue<TestMessage> queue("mq_missing_slash");
    auto result{queue.init(4)};
    EXPECT_FALSE(result);
    EXPECT_EQ(PlatformCore::MessageQueueError::InvalidParameter,
              result.error());
}

TEST(PosixMessageQueueTests, SendReceiveRoundTrip) {
    const auto queue_name{UniqueQueueName()};
    PlatformCore::MessageQueue<TestMessage> queue(queue_name.c_str());
    ASSERT_TRUE(queue.init(4));

    TestMessage tx{.value = 42U};
    ASSERT_TRUE(queue.send(tx, 0));

    TestMessage rx{};
    EXPECT_TRUE(queue.receive(rx, 0));
    EXPECT_EQ(tx.value, rx.value);
}

TEST(PosixMessageQueueTests, SendTimesOutWhenQueueIsFull) {
    const auto queue_name{UniqueQueueName()};
    PlatformCore::MessageQueue<TestMessage> queue(queue_name.c_str());
    ASSERT_TRUE(queue.init(1));

    TestMessage first{.value = 1U};
    ASSERT_TRUE(queue.send(first, 0));

    TestMessage second{.value = 2U};
    auto result{queue.send(second, 10U)};
    EXPECT_FALSE(result);
    EXPECT_EQ(PlatformCore::MessageQueueError::Timeout, result.error());
}
