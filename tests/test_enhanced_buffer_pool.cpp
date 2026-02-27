#include "test_framework.h"
#include "psymp3.h"
#include "io/EnhancedBufferPool.h"
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>

using namespace PsyMP3::IO;

class EnhancedBufferPoolTest : public TestFramework::TestCase {
public:
    EnhancedBufferPoolTest(const std::string& name) : TestCase(name) {}

    void setUp() override {
        // Clear the pool before each test
        EnhancedBufferPool::getInstance().clear();
        EnhancedBufferPool::getInstance().setMemoryPressure(0);
    }

    void tearDown() override {
        EnhancedBufferPool::getInstance().clear();
    }
};

class SingletonTest : public EnhancedBufferPoolTest {
public:
    SingletonTest() : EnhancedBufferPoolTest("SingletonTest") {}
    void runTest() override {
        EnhancedBufferPool& pool1 = EnhancedBufferPool::getInstance();
        EnhancedBufferPool& pool2 = EnhancedBufferPool::getInstance();
        ASSERT_EQUALS(&pool1, &pool2, "getInstance() should return the same instance");
    }
};

class BasicAllocationTest : public EnhancedBufferPoolTest {
public:
    BasicAllocationTest() : EnhancedBufferPoolTest("BasicAllocationTest") {}
    void runTest() override {
        EnhancedBufferPool& pool = EnhancedBufferPool::getInstance();

        // Test small buffer
        auto buf1 = pool.getBuffer(1024);
        ASSERT_TRUE(buf1.capacity() >= 1024, "Buffer capacity should be at least requested size");

        // Test medium buffer
        auto buf2 = pool.getBuffer(32 * 1024);
        ASSERT_TRUE(buf2.capacity() >= 32 * 1024, "Buffer capacity should be at least requested size");

        // Test large buffer
        auto buf3 = pool.getBuffer(256 * 1024);
        ASSERT_TRUE(buf3.capacity() >= 256 * 1024, "Buffer capacity should be at least requested size");
    }
};

class BufferReuseTest : public EnhancedBufferPoolTest {
public:
    BufferReuseTest() : EnhancedBufferPoolTest("BufferReuseTest") {}
    void runTest() override {
        EnhancedBufferPool& pool = EnhancedBufferPool::getInstance();

        // Initial state - clear might not reset hit/miss counts, so we take baseline
        auto stats = pool.getStats();
        size_t initial_hits = stats.buffer_hits;

        // Get a buffer
        size_t size = 4096;
        auto buf = pool.getBuffer(size);
        size_t capacity = buf.capacity();

        // Return it
        pool.returnBuffer(std::move(buf));

        // Verify it's in the pool
        stats = pool.getStats();
        ASSERT_EQUALS(size_t(1), stats.total_buffers, "Pool should have 1 buffer");

        // Get another buffer of same size
        auto buf2 = pool.getBuffer(size);

        // Verify reuse
        stats = pool.getStats();
        ASSERT_TRUE(stats.buffer_hits > initial_hits, "Buffer hits should increase");
        ASSERT_EQUALS(capacity, buf2.capacity(), "Should get the same capacity buffer");
    }
};

class MemoryPressureTest : public EnhancedBufferPoolTest {
public:
    MemoryPressureTest() : EnhancedBufferPoolTest("MemoryPressureTest") {}
    void runTest() override {
        EnhancedBufferPool& pool = EnhancedBufferPool::getInstance();

        // Fill pool with some buffers
        std::vector<std::vector<uint8_t>> buffers;
        for (int i = 0; i < 10; ++i) {
            buffers.push_back(pool.getBuffer(1024));
        }

        // Return them all
        for (auto& buf : buffers) {
            pool.returnBuffer(std::move(buf));
        }

        auto stats = pool.getStats();
        ASSERT_EQUALS(size_t(10), stats.total_buffers, "Should have 10 buffers");

        // Increase memory pressure
        pool.setMemoryPressure(80);

        // Verify pressure is set
        ASSERT_EQUALS(80, pool.getMemoryPressure(), "Memory pressure should be 80");

        // The pool clears buffers immediately if pressure > 70

        stats = pool.getStats();
        ASSERT_TRUE(stats.total_buffers < 10, "Pool size should reduce under pressure");
    }
};

class StatsTest : public EnhancedBufferPoolTest {
public:
    StatsTest() : EnhancedBufferPoolTest("StatsTest") {}
    void runTest() override {
        EnhancedBufferPool& pool = EnhancedBufferPool::getInstance();

        // Initial check - should be empty because setUp clears it
        auto stats = pool.getStats();
        ASSERT_EQUALS(size_t(0), stats.total_buffers, "Should have 0 buffers initially");

        // Get and return a buffer
        pool.returnBuffer(pool.getBuffer(1024));

        stats = pool.getStats();
        ASSERT_EQUALS(size_t(1), stats.total_buffers, "Should have 1 buffer");
        ASSERT_TRUE(stats.total_memory_bytes >= 1024, "Total memory should be tracked");
    }
};

int main() {
    TestFramework::TestSuite suite("EnhancedBufferPool Tests");

    suite.addTest(std::make_unique<SingletonTest>());
    suite.addTest(std::make_unique<BasicAllocationTest>());
    suite.addTest(std::make_unique<BufferReuseTest>());
    suite.addTest(std::make_unique<MemoryPressureTest>());
    suite.addTest(std::make_unique<StatsTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) == 0 ? 0 : 1;
}
