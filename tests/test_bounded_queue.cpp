/*
 * test_bounded_queue.cpp - Tests for BoundedQueue
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

// Simple test structure
struct TestItem {
    int value;
    std::string data;
    
    TestItem(int v = 0, const std::string& d = "") : value(v), data(d) {}
};

// Memory calculator for TestItem
size_t calculateTestItemMemory(const TestItem& item) {
    return sizeof(TestItem) + item.data.capacity();
}

void testBasicOperations() {
    std::cout << "Testing basic operations..." << std::endl;
    
    BoundedQueue<TestItem> queue(3, 0, calculateTestItemMemory);
    
    // Test empty queue
    if (!queue.empty()) {
        std::cout << "FAIL: New queue should be empty" << std::endl;
        return;
    }
    
    if (queue.size() != 0) {
        std::cout << "FAIL: New queue size should be 0" << std::endl;
        return;
    }
    
    // Test pushing items
    TestItem item1(1, "first");
    TestItem item2(2, "second");
    TestItem item3(3, "third");
    TestItem item4(4, "fourth");
    
    if (!queue.tryPush(std::move(item1))) {
        std::cout << "FAIL: Should be able to push first item" << std::endl;
        return;
    }
    
    if (!queue.tryPush(std::move(item2))) {
        std::cout << "FAIL: Should be able to push second item" << std::endl;
        return;
    }
    
    if (!queue.tryPush(std::move(item3))) {
        std::cout << "FAIL: Should be able to push third item" << std::endl;
        return;
    }
    
    // Queue should be full now
    if (queue.tryPush(std::move(item4))) {
        std::cout << "FAIL: Should not be able to push fourth item (queue full)" << std::endl;
        return;
    }
    
    if (queue.size() != 3) {
        std::cout << "FAIL: Queue size should be 3, got " << queue.size() << std::endl;
        return;
    }
    
    // Test popping items
    TestItem popped;
    if (!queue.tryPop(popped)) {
        std::cout << "FAIL: Should be able to pop item" << std::endl;
        return;
    }
    
    if (popped.value != 1 || popped.data != "first") {
        std::cout << "FAIL: Popped item should be first item" << std::endl;
        return;
    }
    
    if (queue.size() != 2) {
        std::cout << "FAIL: Queue size should be 2 after pop" << std::endl;
        return;
    }
    
    // Should be able to push again now
    if (!queue.tryPush(std::move(item4))) {
        std::cout << "FAIL: Should be able to push after pop" << std::endl;
        return;
    }
    
    std::cout << "PASS: Basic operations test" << std::endl;
}

void testMemoryLimits() {
    std::cout << "Testing memory limits..." << std::endl;
    
    // Create queue with memory limit of 100 bytes
    BoundedQueue<TestItem> queue(0, 100, calculateTestItemMemory);
    
    // Create items with large data
    TestItem item1(1, std::string(50, 'a'));  // ~50 bytes data
    TestItem item2(2, std::string(50, 'b'));  // ~50 bytes data
    TestItem item3(3, std::string(50, 'c'));  // ~50 bytes data
    
    if (!queue.tryPush(std::move(item1))) {
        std::cout << "FAIL: Should be able to push first item" << std::endl;
        return;
    }
    
    if (!queue.tryPush(std::move(item2))) {
        std::cout << "FAIL: Should be able to push second item" << std::endl;
        return;
    }
    
    // Third item should fail due to memory limit
    if (queue.tryPush(std::move(item3))) {
        std::cout << "FAIL: Should not be able to push third item (memory limit)" << std::endl;
        return;
    }
    
    std::cout << "PASS: Memory limits test" << std::endl;
}

void testClearOperation() {
    std::cout << "Testing clear operation..." << std::endl;
    
    BoundedQueue<TestItem> queue(5, 0, calculateTestItemMemory);
    
    // Add some items
    for (int i = 0; i < 3; i++) {
        TestItem item(i, "item" + std::to_string(i));
        queue.tryPush(std::move(item));
    }
    
    if (queue.size() != 3) {
        std::cout << "FAIL: Queue should have 3 items" << std::endl;
        return;
    }
    
    // Clear the queue
    queue.clear();
    
    if (!queue.empty()) {
        std::cout << "FAIL: Queue should be empty after clear" << std::endl;
        return;
    }
    
    if (queue.size() != 0) {
        std::cout << "FAIL: Queue size should be 0 after clear" << std::endl;
        return;
    }
    
    if (queue.memoryUsage() != 0) {
        std::cout << "FAIL: Memory usage should be 0 after clear" << std::endl;
        return;
    }
    
    std::cout << "PASS: Clear operation test" << std::endl;
}

void testThreadSafety() {
    std::cout << "Testing thread safety..." << std::endl;
    
    BoundedQueue<TestItem> queue(100, 0, calculateTestItemMemory);
    std::atomic<bool> stop_flag{false};
    std::atomic<int> items_pushed{0};
    std::atomic<int> items_popped{0};
    
    // Producer thread
    std::thread producer([&]() {
        int counter = 0;
        while (!stop_flag) {
            TestItem item(counter++, "data" + std::to_string(counter));
            if (queue.tryPush(std::move(item))) {
                items_pushed++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        while (!stop_flag) {
            TestItem item;
            if (queue.tryPop(item)) {
                items_popped++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(150));
        }
    });
    
    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop_flag = true;
    
    producer.join();
    consumer.join();
    
    std::cout << "Items pushed: " << items_pushed << ", Items popped: " << items_popped << std::endl;
    
    if (items_pushed == 0 || items_popped == 0) {
        std::cout << "FAIL: Both threads should have processed items" << std::endl;
        return;
    }
    
    std::cout << "PASS: Thread safety test" << std::endl;
}

void testConfigurationChanges() {
    std::cout << "Testing configuration changes..." << std::endl;
    
    BoundedQueue<TestItem> queue(2, 0, calculateTestItemMemory);
    
    // Fill queue to capacity
    TestItem item1(1, "first");
    TestItem item2(2, "second");
    TestItem item3(3, "third");
    
    queue.tryPush(std::move(item1));
    queue.tryPush(std::move(item2));
    
    // Should be full
    if (queue.tryPush(std::move(item3))) {
        std::cout << "FAIL: Queue should be full" << std::endl;
        return;
    }
    
    // Increase capacity
    queue.setMaxItems(3);
    
    // Should be able to push now
    if (!queue.tryPush(std::move(item3))) {
        std::cout << "FAIL: Should be able to push after increasing capacity" << std::endl;
        return;
    }
    
    if (queue.getMaxItems() != 3) {
        std::cout << "FAIL: Max items should be 3" << std::endl;
        return;
    }
    
    std::cout << "PASS: Configuration changes test" << std::endl;
}

int main() {
    std::cout << "Running BoundedQueue tests..." << std::endl;
    
    testBasicOperations();
    testMemoryLimits();
    testClearOperation();
    testThreadSafety();
    testConfigurationChanges();
    
    std::cout << "All BoundedQueue tests completed." << std::endl;
    return 0;
}