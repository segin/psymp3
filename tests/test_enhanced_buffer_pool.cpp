/*
 * test_enhanced_buffer_pool.cpp - Unit tests for EnhancedBufferPool
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/EnhancedBufferPool.h"
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>

using namespace PsyMP3::IO;
using namespace TestFramework;

// Helper to reset the pool state as much as possible
void resetPool() {
    auto& pool = EnhancedBufferPool::getInstance();
    pool.clear();
    pool.setMemoryPressure(0);
}

class SingletonTest : public TestCase {
public:
    SingletonTest() : TestCase("EnhancedBufferPool::getInstance") {}

    void runTest() override {
        auto& instance1 = EnhancedBufferPool::getInstance();
        auto& instance2 = EnhancedBufferPool::getInstance();

        ASSERT_EQUALS(&instance1, &instance2, "getInstance should return the same instance");
    }
};

class BasicAllocationTest : public TestCase {
public:
    BasicAllocationTest() : TestCase("EnhancedBufferPool::getBuffer/returnBuffer") {}

    void setUp() override {
        resetPool();
    }

    void runTest() override {
        auto& pool = EnhancedBufferPool::getInstance();
        size_t size = 1024;

        // 1. Get a buffer
        auto buffer1 = pool.getBuffer(size);
        size_t capacity1 = buffer1.capacity();
        ASSERT_TRUE(capacity1 >= size, "Buffer capacity should be at least requested size");

        // 2. Return the buffer
        pool.returnBuffer(std::move(buffer1));

        // 3. Get another buffer of same size
        auto buffer2 = pool.getBuffer(size);

        // 4. Verify reuse (capacity should match)
        ASSERT_EQUALS(capacity1, buffer2.capacity(), "Should reuse buffer of same capacity");
    }
};

class BufferCategoriesTest : public TestCase {
public:
    BufferCategoriesTest() : TestCase("EnhancedBufferPool Categories") {}

    void setUp() override {
        resetPool();
    }

    void runTest() override {
        auto& pool = EnhancedBufferPool::getInstance();

        // Small buffer (< 16KB)
        auto small = pool.getBuffer(1024);
        ASSERT_TRUE(small.capacity() >= 1024, "Small buffer capacity");
        pool.returnBuffer(std::move(small));

        // Medium buffer (16KB - 128KB)
        auto medium = pool.getBuffer(32 * 1024);
        ASSERT_TRUE(medium.capacity() >= 32 * 1024, "Medium buffer capacity");
        pool.returnBuffer(std::move(medium));

        // Large buffer (> 128KB)
        auto large = pool.getBuffer(256 * 1024);
        ASSERT_TRUE(large.capacity() >= 256 * 1024, "Large buffer capacity");
        pool.returnBuffer(std::move(large));

        // Verify they are pooled
        auto stats = pool.getStats();
        ASSERT_TRUE(stats.total_buffers >= 3, "Should have at least 3 buffers pooled");
    }
};

class MemoryPressureTest : public TestCase {
public:
    MemoryPressureTest() : TestCase("EnhancedBufferPool Memory Pressure") {}

    void setUp() override {
        resetPool();
    }

    void runTest() override {
        auto& pool = EnhancedBufferPool::getInstance();

        // Set high pressure
        pool.setMemoryPressure(100);
        ASSERT_EQUALS(100, pool.getMemoryPressure(), "Pressure should be 100");

        // Under high pressure (100%), max buffer size drops to 256KB
        // Try to pool a large buffer > 256KB (e.g. 512KB)

        // Use a manually created vector to verify returnBuffer logic directly
        // because getBuffer might not even allocate it via pool logic if strictly following max size limits
        size_t huge_size = 512 * 1024;
        std::vector<uint8_t> huge_buffer;
        huge_buffer.reserve(huge_size);

        size_t initial_count = pool.getStats().total_buffers;

        // Return it - should be rejected due to high pressure constraints on size
        pool.returnBuffer(std::move(huge_buffer));

        ASSERT_EQUALS(initial_count, pool.getStats().total_buffers, "Huge buffer should not be pooled under high pressure");

        // Lower pressure
        pool.setMemoryPressure(0);
        // Now it should be poolable (if < 1MB, which 512KB is)

        std::vector<uint8_t> acceptable_buffer;
        acceptable_buffer.reserve(huge_size);

        pool.returnBuffer(std::move(acceptable_buffer));
        ASSERT_EQUALS(initial_count + 1, pool.getStats().total_buffers, "Buffer should be pooled under low pressure");
    }
};

class StatsTest : public TestCase {
public:
    StatsTest() : TestCase("EnhancedBufferPool::getStats") {}

    void setUp() override {
        resetPool();
    }

    void runTest() override {
        auto& pool = EnhancedBufferPool::getInstance();
        pool.clear(); // Ensure 0 buffers

        auto stats = pool.getStats();
        ASSERT_EQUALS((size_t)0, stats.total_buffers, "Should have 0 buffers initially");

        auto buf = pool.getBuffer(1024);
        pool.returnBuffer(std::move(buf));

        stats = pool.getStats();
        ASSERT_EQUALS((size_t)1, stats.total_buffers, "Should have 1 buffer after return");
        ASSERT_TRUE(stats.total_memory_bytes >= 1024, "Total memory should track buffer size");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("EnhancedBufferPool Unit Tests");

    suite.addTest(std::make_unique<SingletonTest>());
    suite.addTest(std::make_unique<BasicAllocationTest>());
    suite.addTest(std::make_unique<BufferCategoriesTest>());
    suite.addTest(std::make_unique<MemoryPressureTest>());
    suite.addTest(std::make_unique<StatsTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
