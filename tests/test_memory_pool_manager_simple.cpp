/*
 * test_memory_pool_manager_simple.cpp - Simple test for MemoryPoolManager threading safety
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <thread>
#include <vector>
#include <atomic>

/**
 * Simple test that allocates and immediately releases buffers
 */
void test_simple_threading() {
    std::cout << "Testing simple threading safety..." << std::endl;
    
    MemoryPoolManager& manager = MemoryPoolManager::getInstance();
    manager.initializePools();
    
    const int num_threads = 4;
    const int operations_per_thread = 50;
    std::atomic<int> errors{0};
    std::atomic<int> successful_operations{0};
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            std::string component_name = "test_component_" + std::to_string(i);
            
            for (int j = 0; j < operations_per_thread; ++j) {
                try {
                    // Allocate buffer
                    size_t size = 32 * 1024; // Fixed 32KB size
                    uint8_t* buffer = manager.allocateBuffer(size, component_name);
                    
                    if (buffer) {
                        // Write to buffer to verify it's valid
                        buffer[0] = 0xAA;
                        buffer[size - 1] = 0xBB;
                        
                        // Immediately release the buffer
                        manager.releaseBuffer(buffer, size, component_name);
                        successful_operations++;
                    } else {
                        errors++;
                    }
                } catch (...) {
                    errors++;
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Simple threading test completed:" << std::endl;
    std::cout << "  Successful operations: " << successful_operations.load() << std::endl;
    std::cout << "  Errors: " << errors.load() << std::endl;
    
    // Get final memory stats
    auto stats = manager.getMemoryStats();
    std::cout << "  Final total allocated: " << stats["total_allocated"] << " bytes" << std::endl;
    
    if (errors.load() == 0) {
        std::cout << "✓ Simple threading test PASSED" << std::endl;
    } else {
        std::cout << "✗ Simple threading test FAILED" << std::endl;
    }
}

/**
 * Test concurrent access to different methods
 */
void test_method_concurrency() {
    std::cout << "\nTesting method concurrency..." << std::endl;
    
    MemoryPoolManager& manager = MemoryPoolManager::getInstance();
    
    std::atomic<int> errors{0};
    std::atomic<bool> stop_flag{false};
    
    std::vector<std::thread> threads;
    
    // Thread 1: Allocate/release operations
    threads.emplace_back([&]() {
        std::string component_name = "concurrent_test";
        
        while (!stop_flag.load()) {
            try {
                uint8_t* buffer = manager.allocateBuffer(16 * 1024, component_name);
                if (buffer) {
                    buffer[0] = 0xCC;
                    manager.releaseBuffer(buffer, 16 * 1024, component_name);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (...) {
                errors++;
            }
        }
    });
    
    // Thread 2: Stats access
    threads.emplace_back([&]() {
        while (!stop_flag.load()) {
            try {
                auto stats = manager.getMemoryStats();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            } catch (...) {
                errors++;
            }
        }
    });
    
    // Thread 3: Optimization calls
    threads.emplace_back([&]() {
        while (!stop_flag.load()) {
            try {
                manager.optimizeMemoryUsage();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (...) {
                errors++;
            }
        }
    });
    
    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop_flag.store(true);
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Method concurrency test completed:" << std::endl;
    std::cout << "  Errors: " << errors.load() << std::endl;
    
    if (errors.load() == 0) {
        std::cout << "✓ Method concurrency test PASSED" << std::endl;
    } else {
        std::cout << "✗ Method concurrency test FAILED" << std::endl;
    }
}

int main() {
    std::cout << "MemoryPoolManager Simple Threading Test" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    try {
        test_simple_threading();
        test_method_concurrency();
        
        std::cout << "\nAll simple threading tests completed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}