/*
 * test_bounded_buffer.cpp - Unit tests for BoundedBuffer and BoundedCircularBuffer
 * This file is part of PsyMP3.
 */

#include "psymp3.h"
#include "test_framework.h"
#include <iostream>
#include <cstring>
#include <vector>

using namespace TestFramework;
using namespace PsyMP3::IO;

class TestBoundedBuffer : public TestCase {
public:
    TestBoundedBuffer() : TestCase("BoundedBuffer Tests") {}

    void setUp() override {
        MemoryPoolManager::getInstance().initializePools();
    }

    void runTest() override {
        // 1. Construction
        {
            BoundedBuffer buffer(1024, 256);
            ASSERT_EQUALS(0u, buffer.size(), "Initial size should be 0");
            ASSERT_EQUALS(256u, buffer.capacity(), "Initial capacity should be 256");
            ASSERT_EQUALS(1024u, buffer.max_size(), "Max size should be 1024");
            ASSERT_TRUE(buffer.empty(), "Buffer should be empty");
        }

        // 2. Resize and Reserve
        {
            BoundedBuffer buffer(1024);
            ASSERT_TRUE(buffer.resize(512), "Resize to 512 should succeed");
            ASSERT_EQUALS(512u, buffer.size(), "Size should be 512");
            ASSERT_TRUE(buffer.capacity() >= 512u, "Capacity should be at least 512");

            ASSERT_TRUE(buffer.reserve(1024), "Reserve 1024 should succeed");
            ASSERT_EQUALS(1024u, buffer.capacity(), "Capacity should be 1024");

            ASSERT_FALSE(buffer.resize(2048), "Resize beyond max_size should fail");
            ASSERT_FALSE(buffer.reserve(2048), "Reserve beyond max_size should fail");
        }

        // 3. Append and Data integrity
        {
            BoundedBuffer buffer(100);
            const char* msg1 = "Hello";
            ASSERT_TRUE(buffer.append(msg1, 5), "Append 'Hello' should succeed");
            ASSERT_EQUALS(5u, buffer.size(), "Size should be 5");
            ASSERT_EQUALS(0, std::memcmp(buffer.data(), msg1, 5), "Data should match 'Hello'");

            const char* msg2 = " World";
            ASSERT_TRUE(buffer.append(msg2, 6), "Append ' World' should succeed");
            ASSERT_EQUALS(11u, buffer.size(), "Size should be 11");
            ASSERT_EQUALS(0, std::memcmp(buffer.data(), "Hello World", 11), "Data should match 'Hello World'");
        }

        // 4. Set
        {
            BoundedBuffer buffer(100);
            const char* msg = "PsyMP3";
            ASSERT_TRUE(buffer.set(msg, 6), "Set 'PsyMP3' should succeed");
            ASSERT_EQUALS(6u, buffer.size(), "Size should be 6");
            ASSERT_EQUALS(0, std::memcmp(buffer.data(), msg, 6), "Data should match 'PsyMP3'");

            const char* msg2 = "Test";
            ASSERT_TRUE(buffer.set(msg2, 4), "Set 'Test' should succeed");
            ASSERT_EQUALS(4u, buffer.size(), "Size should be 4");
            ASSERT_EQUALS(0, std::memcmp(buffer.data(), msg2, 4), "Data should match 'Test'");
        }

        // 5. Copy To
        {
            BoundedBuffer buffer(100);
            buffer.set("0123456789", 10);
            char dest[11];
            std::memset(dest, 0, 11);

            size_t copied = buffer.copy_to(dest, 2, 5);
            ASSERT_EQUALS(5u, copied, "Should copy 5 bytes");
            ASSERT_EQUALS(0, std::memcmp(dest, "23456", 5), "Copied data should match '23456'");

            copied = buffer.copy_to(dest, 8, 5);
            ASSERT_EQUALS(2u, copied, "Should only copy 2 bytes (offset 8, size 10)");
            ASSERT_EQUALS(0, std::memcmp(dest, "89", 2), "Copied data should match '89'");
        }

        // 6. Memory management
        {
            BoundedBuffer buffer(1024, 512);
            buffer.set("data", 4);
            ASSERT_EQUALS(512u, buffer.capacity(), "Capacity should still be 512");

            buffer.shrink_to_fit();
            ASSERT_EQUALS(4u, buffer.capacity(), "Capacity should be 4 after shrink_to_fit");
            ASSERT_EQUALS(4u, buffer.size(), "Size should still be 4");

            buffer.clear();
            ASSERT_EQUALS(0u, buffer.size(), "Size should be 0 after clear");
            ASSERT_EQUALS(4u, buffer.capacity(), "Capacity should remain after clear");
            ASSERT_TRUE(buffer.empty(), "Buffer should be empty after clear");
        }

        // 7. Stats
        {
            BoundedBuffer buffer(1000, 500);
            auto stats = buffer.getStats();
            ASSERT_EQUALS(0u, stats["current_size"], "Stats current_size should be 0");
            ASSERT_EQUALS(500u, stats["current_capacity"], "Stats current_capacity should be 500");
            ASSERT_EQUALS(1000u, stats["max_size"], "Stats max_size should be 1000");
        }
    }
};

class TestBoundedCircularBuffer : public TestCase {
public:
    TestBoundedCircularBuffer() : TestCase("BoundedCircularBuffer Tests") {}

    void setUp() override {
        MemoryPoolManager::getInstance().initializePools();
    }

    void runTest() override {
        // 1. Construction
        {
            BoundedCircularBuffer cb(10);
            ASSERT_EQUALS(10u, cb.capacity(), "Capacity should be 10");
            ASSERT_EQUALS(0u, cb.available(), "Available should be 0");
            ASSERT_EQUALS(10u, cb.space(), "Space should be 10");
            ASSERT_TRUE(cb.empty(), "Buffer should be empty");
            ASSERT_FALSE(cb.full(), "Buffer should not be full");
        }

        // 2. Write and Read
        {
            BoundedCircularBuffer cb(10);
            const char* data = "abcde";
            size_t written = cb.write(data, 5);
            ASSERT_EQUALS(5u, written, "Should write 5 bytes");
            ASSERT_EQUALS(5u, cb.available(), "Available should be 5");
            ASSERT_EQUALS(5u, cb.space(), "Space should be 5");

            char dest[6];
            std::memset(dest, 0, 6);
            size_t read_bytes = cb.read(dest, 3);
            ASSERT_EQUALS(3u, read_bytes, "Should read 3 bytes");
            ASSERT_EQUALS(0, std::memcmp(dest, "abc", 3), "Data should match 'abc'");
            ASSERT_EQUALS(2u, cb.available(), "Available should be 2");
        }

        // 3. Wrap around logic
        {
            BoundedCircularBuffer cb(10);
            cb.write("0123456789", 10);
            ASSERT_TRUE(cb.full(), "Buffer should be full");

            char dest[6];
            cb.read(dest, 5); // read 01234, read_pos is 5
            ASSERT_EQUALS(5u, cb.available(), "Available should be 5");
            ASSERT_EQUALS(5u, cb.space(), "Space should be 5");

            size_t written = cb.write("ABCDE", 5); // write ABCDE at end and wrap around
            ASSERT_EQUALS(5u, written, "Should write 5 bytes");
            ASSERT_TRUE(cb.full(), "Buffer should be full again");

            char dest2[11];
            std::memset(dest2, 0, 11);
            cb.read(dest2, 10); // should read 56789ABCDE
            ASSERT_EQUALS(0, std::memcmp(dest2, "56789ABCDE", 10), "Wrapped data integrity check");
        }

        // 4. Peek and Skip
        {
            BoundedCircularBuffer cb(10);
            cb.write("12345", 5);

            char dest[6];
            std::memset(dest, 0, 6);
            size_t peeked = cb.peek(dest, 3);
            ASSERT_EQUALS(3u, peeked, "Should peek 3 bytes");
            ASSERT_EQUALS(0, std::memcmp(dest, "123", 3), "Peek data should match '123'");
            ASSERT_EQUALS(5u, cb.available(), "Available should still be 5 after peek");

            size_t skipped = cb.skip(2);
            ASSERT_EQUALS(2u, skipped, "Should skip 2 bytes");
            ASSERT_EQUALS(3u, cb.available(), "Available should be 3 after skip");

            cb.read(dest, 3);
            ASSERT_EQUALS(0, std::memcmp(dest, "345", 3), "Remaining data should be '345'");
        }

        // 5. Clear and Stats
        {
            BoundedCircularBuffer cb(10);
            cb.write("data", 4);
            cb.clear();
            ASSERT_TRUE(cb.empty(), "Buffer should be empty after clear");
            ASSERT_EQUALS(0u, cb.available(), "Available should be 0 after clear");

            cb.write("test", 4);
            auto stats = cb.getStats();
            ASSERT_EQUALS(10u, stats["capacity"], "Stats capacity should be 10");
            ASSERT_EQUALS(4u, stats["available"], "Stats available should be 4");
        }
    }
};

int main() {
    TestSuite suite("BoundedBuffer Tests");

    suite.addTest(std::make_unique<TestBoundedBuffer>());
    suite.addTest(std::make_unique<TestBoundedCircularBuffer>());

    auto results = suite.runAll();
    suite.printResults(results);

    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
