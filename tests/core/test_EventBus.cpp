#include <gtest/gtest.h>
#include "core/EventBus.h"

using namespace FaluEngine;

struct TestEvent { int value; };

TEST(EventBus, SubscribeAndPublish) {
    int received = -1;

    EventBus::get().subscribe<TestEvent>([&](const TestEvent& e) {
        received = e.value;
    });

    EventBus::get().publish(TestEvent{ 42 });
    EXPECT_EQ(received, 42);
}

TEST(EventBus, Unsubscribe) {
    int count = 0;

    auto id = EventBus::get().subscribe<TestEvent>([&](const TestEvent&) { ++count; });
    EventBus::get().publish(TestEvent{ 1 });
    EXPECT_EQ(count, 1);

    EventBus::get().unsubscribe<TestEvent>(id);
    EventBus::get().publish(TestEvent{ 2 });
    EXPECT_EQ(count, 1); // アンサブ後は増えない
}

TEST(EventBus, MultipleSubscribers) {
    int a = 0, b = 0;

    EventBus::get().subscribe<TestEvent>([&](const TestEvent&) { ++a; });
    EventBus::get().subscribe<TestEvent>([&](const TestEvent&) { ++b; });
    EventBus::get().publish(TestEvent{ 0 });

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);
}
