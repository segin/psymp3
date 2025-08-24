/*
 * test_memory_pool_manager_threading.cpp - Test MemoryPoolManager threading safety
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

/**
 * Test concurrent allocation and deallocation operations
 */
void test_concurrent_allocation() {
    std::cout << "Testing concurrent allocation/deallocation..." << std::endl;
    
    MemoryPoolManager& manager = MemoryPoolManager::getInstance();
    manager.initializePools();
    
    const int num_threads = 8;
    const int operations_per_thread = 100;
    std::atomic<int> errors{0};
    std::atomic<int> successful_allocations{0};
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            std::string component_name = "test_component_" + std::to_string(i);
            std::vector<std::pair<uint8_t*, size_t>> allocated_buffers;
            
            for (int j = 0; j < operations_per_thread; ++j) {
                try {
                    // Allocate various sizes
                    size_t size = (j % 8 + 1) * 8 * 1024; // 8KB to 64KB
                    uint8_t* buffer = manager.allocateBuffer(size, component_name);
                    
                    if (buffer) {
                        successful_allocations++;
                        allocated_buffers.push_back({buffer, size});
                        
                        // Write to buffer to ensure it's valid
                        memset(buffer, 0xAA, size);
                        
                        // Occasionally release some buffers
                        if (allocated_buffers.size() > 10) {
                            auto& to_release = allocated_buffers.back();
                            manager.releaseBuffer(to_release.first, to_release.second, component_name);
                            allocated_buffers.pop_back();
                        }
                    }
                } catch (...) {
                    errors++;
                }
            }
            
            // Release all remaining buffers
            for (auto& buffer_info : allocated_buffers) {
                try {
                    manager.releaseBuffer(buffer_info.first, buffer_info.second, component_name);
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
    
    std::cout << "Concurrent allocation test completed:" << std::endl;
    std::cout << "  Successful allocations: " << successful_allocations.load() << std::endl;
    std::cout << "  Errors: " << errors.load() << std::endl;
    
    // Get final memory stats
    auto stats = manager.getMemoryStats();
    std::cout << "  Final total allocated: " << stats["total_allocated"] << " bytes" << std::endl;
    std::cout << "  Final total pooled: " << stats["total_pooled"] << " bytes" << std::endl;
    
    if (errors.load() == 0) {
        std::cout << "✓ Concurrent allocation test PASSED" << std::endl;
    } else {
        std::cout << "✗ Concurrent allocation test FAILED" << std::endl;
    }
}

/**
 * Test concurrent access to memory statistics
 */
void test_concurrent_stats_access() {
    std::cout << "\nTesting concurrent stats access..." << std::endl;
    
    MemoryPoolManager& manager = MemoryPoolManager::getInstance();
    
    const int num_threads = 4;
    const int stats_calls_per_thread = 50;
    std::atomic<int> errors{0};
    std::atomic<int> successful_calls{0};
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < stats_calls_per_thread; ++j) {
                try {
                    auto stats = manager.getMemoryStats();
                    
                    // Verify stats contain expected keys
                    if (stats.find("total_allocated") != stats.end() &&
                        stats.find("total_pooled") != stats.end() &&
                        stats.find("memory_pressure") != stats.end()) {
                        successful_calls++;
                    } else {
                        errors++;
                    }
                    
                    // Small delay to increase contention
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
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
    
    std::cout << "Concurrent stats access test completed:" << std::endl;
    std::cout << "  Successful calls: " << successful_calls.load() << std::endl;
    std::cout << "  Errors: " << errors.load() << std::endl;
    
    if (errors.load() == 0) {
        std::cout << "✓ Concurrent stats access test PASSED" << std::endl;
    } else {
        std::cout << "✗ Concurrent stats access test FAILED" << std::endl;
    }
}

/**
 * Test concurrent memory optimization
 */
void test_concurrent_optimization() {
    std::cout << "\nTesting concurrent memory optimization..." << std::endl;
    
    MemoryPoolManager& manager = MemoryPoolManager::getInstance();
    
    const int num_threads = 3;
    std::atomic<int> errors{0};
    std::atomic<bool> stop_flag{false};
    
    std::vector<std::thread> threads;
    
    // Thread 1: Continuous allocation/deallocation
    threads.emplace_back([&]() {
        std::string component_name = "optimization_test";
        std::vector<std::pair<uint8_t*, size_t>> buffers;
        
        while (!stop_flag.load()) {
            try {
                // Allocate
                size_t size = 32 * 1024; // 32KB
                uint8_t* buffer = manager.allocateBuffer(size, component_name);
                if (buffer) {
                    buffers.push_back({buffer, size});
                }
                
                // Occasionally release some buffers
                if (buffers.size() > 5) {
                    auto& to_release = buffers.back();
                    manager.releaseBuffer(to_release.first, to_release.second, component_name);
                    buffers.pop_back();
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (...) {
                errors++;
            }
        }
        
        // Clean up remaining buffers
        for (auto& buffer_info : buffers) {
            try {
                manager.releaseBuffer(buffer_info.first, buffer_info.second, component_name);
            } catch (...) {
                errors++;
            }
        }
    });
    
    // Thread 2: Continuous optimization calls
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
    
    // Thread 3: Continuous stats access
    threads.emplace_back([&]() {
        while (!stop_flag.load()) {
            try {
                auto stats = manager.getMemoryStats();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            } catch (...) {
                errors++;
            }
        }
    });
    
    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop_flag.store(true);
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Concurrent optimization test completed:" << std::endl;
    std::cout << "  Errors: " << errors.load() << std::endl;
    
    if (errors.load() == 0) {
        std::cout << "✓ Concurrent optimization test PASSED" << std::endl;
    } else {
        std::cout << "✗ Concurrent optimization test FAILED" << std::endl;
    }
}

int main() {
    std::cout << "MemoryPoolManager Threading Safety Test" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    try {
        test_concurrent_allocation();
        test_concurrent_stats_access();
        test_concurrent_optimization();
        
        std::cout << "\nAll threading safety tests completed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}