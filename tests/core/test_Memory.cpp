#include <gtest/gtest.h>
#include "core/Memory/PoolAllocator.h"

using namespace FaluEngine;

TEST(PoolAllocator, AllocateAndDeallocate) {
    PoolAllocator pool(64, 8);

    EXPECT_EQ(pool.freeCount(), 8u);

    void* p = pool.allocate();
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(pool.freeCount(), 7u);

    pool.deallocate(p);
    EXPECT_EQ(pool.freeCount(), 8u);
}

TEST(PoolAllocator, ExhaustPool) {
    PoolAllocator pool(16, 3);

    void* a = pool.allocate();
    void* b = pool.allocate();
    void* c = pool.allocate();
    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    EXPECT_NE(c, nullptr);
    EXPECT_EQ(pool.freeCount(), 0u);

    EXPECT_THROW({ [[maybe_unused]] void* x = pool.allocate(); }, std::bad_alloc);

    pool.deallocate(a);
    EXPECT_EQ(pool.freeCount(), 1u);
}
