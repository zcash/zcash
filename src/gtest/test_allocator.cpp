#include <gtest/gtest.h>

#include "support/allocators/secure.h"

TEST(AllocatorTests, LockedPoolAbortOnDoubleFree) {
    LockedPoolManager &pool = LockedPoolManager::Instance();

    // We should be able to allocate and free memory.
    void *a0 = pool.alloc(16);
    pool.free(a0);

    // Process terminates on double-free.
    EXPECT_DEATH(pool.free(a0), "Arena: invalid or double free");
}

TEST(AllocatorTests, LockedPoolAbortOnFreeInvalidPointer) {
    LockedPoolManager &pool = LockedPoolManager::Instance();
    bool notInPool = false;

    // Process terminates if we try to free memory that wasn't allocated by the pool.
    EXPECT_DEATH(pool.free(&notInPool), "LockedPool: invalid address not pointing to any arena");
}
