/*
 * test_memory_pool_manager_thread_safety_comprehensive.cpp - Comprehensive thread safety tests for MemoryPoolManager
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <mutex>

class MemoryPoolManagerThreadSafetyTest {
public:
    void testConcurrentBufferOperations() {
        Debug::log("test", "MemoryPoolManagerThreadSafetyTest::testConcurrentBufferOperations() - Starting concurrent buffer operations test");
        
        auto& pool_manager = MemoryPoolManager::getInstance();
        pool_manager.initializePools();
        
        const int num_threads = 8;
        const int operations_per_thread = 200;
        std::atomic<int> successful_allocations{0};
        std::atomic<int> successful_releases{0};
        std::atomic<int> allocation_failures{0};
        std::atomic<int> exceptions{0};
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&, i]() {
                std::vector<std::pair<uint8_t*, size_t>> allocated_buffers;
                std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count() + i);
                std::uniform_int_distribution<size_t> size_dist(1024, 128 * 1024); // 1KB to 128KB
                
                std::string component_name = "thread_" + std::to_string(i);
                
                for (int j = 0; j < operations_per_thread; j++) {
                    try {
                        // Randomly choose operation: allocate (70%) or release (30%)
                        if (allocated_buffers.empty() || (rng() % 10) < 7) {
                            // Allocate buffer
                            size_t size = size_dist(rng);
                            uint8_t* buffer = pool_manager.allocateBuffer(size, component_name);
                            
                            if (buffer) {
                                // Verify buffer is writable
                                buffer[0] = static_cast<uint8_t>(i);
                                buffer[size - 1] = static_cast<uint8_t>(j);
                                
                                allocated_buffers.push_back({buffer, size});
                                successful_allocations++;
                            } else {
                                allocation_failures++;
                            }
                        } else {
                            // Release buffer
                            if (!allocated_buffers.empty()) {
                                size_t index = rng() % allocated_buffers.size();
                                auto buffer_info = allocated_buffers[index];
                                allocated_buffers.erase(allocated_buffers.begin() + index);
                                
                                // Verify buffer still contains our data
                                ASSERT_EQUALS(buffer_info.first[0], static_cast<uint8_t>(i), "Buffer data corrupted");
                                ASSERT_EQUALS(buffer_info.first[buffer_info.second - 1], static_cast<uint8_t>(j % 256), "Buffer end data corrupted");
                                
                                pool_manager.releaseBuffer(buffer_info.first, buffer_info.second, component_name);
                                successful_releases++;
                            }
                        }
                        
                        // Occasionally yield to increase contention
                        if (j % 10 == 0) {
                            std::this_thread::yield();
                        }
                        
                    } catch (...) {
                        exceptions++;
                    }
                }
                
                // Clean up remaining buffers
                for (auto& buffer_info : allocated_buffers) {
                    try {
                        pool_manager.releaseBuffer(buffer_info.first, buffer_info.second, component_name);
                        successful_releases++;
                    } catch (...) {
                        exceptions++;
                    }
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Verify results
        ASSERT_TRUE(successful_allocations.load() > 0, "Should have successful allocations");
        ASSERT_TRUE(successful_releases.load() > 0, "Should have successful releases");
        ASSERT_EQUALS(exceptions.load(), 0, "Should not have any exceptions");
        
        Debug::log("test", "Concurrent buffer operations test completed - allocations: ", successful_allocations.load(), 
                  ", releases: ", successful_releases.load(), ", failures: ", allocation_failures.load(), 
                  ", exceptions: ", exceptions.load());
    }
    
    void testCallbackDeadlockPrevention() {
        Debug::log("test", "MemoryPoolManagerThreadSafetyTest::testCallbackDeadlockPrevention() - Testing callback deadlock prevention");
        
        auto& pool_manager = MemoryPoolManager::getInstance();
        
        std::atomic<int> callback_executions{0};
        std::atomic<int> nested_operations{0};
        std::atomic<int> deadlocks_detected{0};
        std::atomic<bool> test_running{true};
        
        // Register callbacks that try to perform operations that could cause deadlocks
        std::vector<int> callback_ids;
        
        for (int i = 0; i < 3; i++) {
            int callback_id = pool_manager.registerMemoryPressureCallback([&, i](int pressure) {
                callback_executions++;
                
                try {
                    // Try operations that could cause deadlocks
                    switch (i) {
                        case 0: {
                            // Try to allocate memory from within callback
                            uint8_t* buffer = pool_manager.allocateBuffer(1024, "callback_test");
                            if (buffer) {
                                pool_manager.releaseBuffer(buffer, 1024, "callback_test");
                                nested_operations++;
                            }
                            break;
                        }
                        case 1: {
                            // Try to get memory stats
                            auto stats = pool_manager.getMemoryStats();
                            nested_operations++;
                            break;
                        }
                        case 2: {
                            // Try to optimize memory usage
                            pool_manager.optimizeMemoryUsage();
                            nested_operations++;
                            break;
                        }
                    }
                } catch (...) {
                    deadlocks_detected++;
                }
            });
            
            callback_ids.push_back(callback_id);
        }
        
        // Create threads that trigger operations that might call callbacks
        std::vector<std::thread> threads;
        
        for (int i = 0; i < 4; i++) {
            threads.emplace_back([&, i]() {
                std::string component_name = "deadlock_test_" + std::to_string(i);
                
                while (test_running) {
                    try {
                        // Perform various operations that might trigger callbacks
                        switch (i % 4) {
                            case 0: {
                                // Allocate and release buffers
                                uint8_t* buffer = pool_manager.allocateBuffer(32 * 1024, component_name);
                                if (buffer) {
                                    pool_manager.releaseBuffer(buffer, 32 * 1024, component_name);
                                }
                                break;
                            }
                            case 1: {
                                // Optimize memory usage
                                pool_manager.optimizeMemoryUsage();
                                break;
                            }
                            case 2: {
                                // Get memory stats
                                auto stats = pool_manager.getMemoryStats();
                                break;
                            }
                            case 3: {
                                // Check if safe to allocate
                                bool safe = pool_manager.isSafeToAllocate(64 * 1024, component_name);
                                (void)safe; // Suppress unused variable warning
                                break;
                            }
                        }
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        
                    } catch (...) {
                        deadlocks_detected++;
                    }
                }
            });
        }
        
        // Let the test run for a short time
        std::this_thread::sleep_for(std::chrono::seconds(2));
        test_running = false;
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Unregister callbacks
        for (int callback_id : callback_ids) {
            pool_manager.unregisterMemoryPressureCallback(callback_id);
        }
        
        // Verify results
        ASSERT_TRUE(callback_executions.load() > 0, "Should have executed callbacks");
        ASSERT_EQUALS(deadlocks_detected.load(), 0, "Should not have detected any deadlocks");
        
        Debug::log("test", "Callback deadlock prevention test completed - executions: ", callback_executions.load(), 
                  ", nested operations: ", nested_operations.load(), ", deadlocks: ", deadlocks_detected.load());
    }
    
    void testMemoryPressureStress() {
        Debug::log("test", "MemoryPoolManagerThreadSafetyTest::testMemoryPressureStress() - Testing memory pressure stress scenarios");
        
        auto& pool_manager = MemoryPoolManager::getInstance();
        
        // Set lower memory limits to trigger pressure scenarios
        pool_manager.setMemoryLimits(16 * 1024 * 1024, 8 * 1024 * 1024); // 16MB total, 8MB buffer
        
        std::atomic<int> pressure_callbacks{0};
        std::atomic<int> allocation_attempts{0};
        std::atomic<int> allocation_successes{0};
        std::atomic<int> allocation_rejections{0};
        std::atomic<bool> stress_running{true};
        
        // Register pressure callback
        int callback_id = pool_manager.registerMemoryPressureCallback([&](int pressure) {
            pressure_callbacks++;
            Debug::log("test", "Memory pressure callback: ", pressure, "%");
        });
        
        // Create threads that aggressively allocate memory
        std::vector<std::thread> threads;
        const int num_threads = 6;
        
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&, i]() {
                std::vector<std::pair<uint8_t*, size_t>> allocated_buffers;
                std::string component_name = "stress_" + std::to_string(i);
                
                while (stress_running) {
                    try {
                        allocation_attempts++;
                        
                        // Try to allocate large buffers to create memory pressure
                        size_t size = (256 + (i * 64)) * 1024; // 256KB to 640KB
                        
                        if (pool_manager.isSafeToAllocate(size, component_name)) {
                            uint8_t* buffer = pool_manager.allocateBuffer(size, component_name);
                            
                            if (buffer) {
                                allocated_buffers.push_back({buffer, size});
                                allocation_successes++;
                                
                                // Fill buffer to ensure it's really allocated
                                memset(buffer, 0xDD, size);
                            } else {
                                allocation_rejections++;
                            }
                        } else {
                            allocation_rejections++;
                        }
                        
                        // Occasionally release some buffers to create churn
                        if (allocated_buffers.size() > 5 && (allocation_attempts % 10) == 0) {
                            auto buffer_info = allocated_buffers.back();
                            allocated_buffers.pop_back();
                            pool_manager.releaseBuffer(buffer_info.first, buffer_info.second, component_name);
                        }
                        
                        // Trigger optimization occasionally
                        if ((allocation_attempts % 50) == 0) {
                            pool_manager.optimizeMemoryUsage();
                        }
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                        
                    } catch (...) {
                        // Expected under high memory pressure
                    }
                }
                
                // Clean up allocated buffers
                for (auto& buffer_info : allocated_buffers) {
                    try {
                        pool_manager.releaseBuffer(buffer_info.first, buffer_info.second, component_name);
                    } catch (...) {
                        // Ignore cleanup errors
                    }
                }
            });
        }
        
        // Let the stress test run
        std::this_thread::sleep_for(std::chrono::seconds(3));
        stress_running = false;
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Unregister callback
        pool_manager.unregisterMemoryPressureCallback(callback_id);
        
        // Reset memory limits to defaults
        pool_manager.setMemoryLimits(64 * 1024 * 1024, 32 * 1024 * 1024);
        
        // Verify results
        ASSERT_TRUE(allocation_attempts.load() > 100, "Should have attempted many allocations");
        ASSERT_TRUE(allocation_successes.load() > 0, "Should have some successful allocations");
        ASSERT_TRUE(pressure_callbacks.load() > 0, "Should have received pressure callbacks");
        
        Debug::log("test", "Memory pressure stress test completed - attempts: ", allocation_attempts.load(), 
                  ", successes: ", allocation_successes.load(), ", rejections: ", allocation_rejections.load(), 
                  ", pressure callbacks: ", pressure_callbacks.load());
    }
    
    void testCallbackRegistrationConcurrency() {
        Debug::log("test", "MemoryPoolManagerThreadSafetyTest::testCallbackRegistrationConcurrency() - Testing concurrent callback registration");
        
        auto& pool_manager = MemoryPoolManager::getInstance();
        
        std::atomic<int> registrations{0};
        std::atomic<int> unregistrations{0};
        std::atomic<int> callback_calls{0};
        std::atomic<int> errors{0};
        std::atomic<bool> test_running{true};
        
        std::mutex callback_ids_mutex;
        std::vector<int> active_callback_ids;
        
        // Thread that continuously registers and unregisters callbacks
        std::thread registration_thread([&]() {
            while (test_running) {
                try {
                    // Register a callback
                    int callback_id = pool_manager.registerMemoryPressureCallback([&](int pressure) {
                        callback_calls++;
                    });
                    
                    {
                        std::lock_guard<std::mutex> lock(callback_ids_mutex);
                        active_callback_ids.push_back(callback_id);
                    }
                    registrations++;
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    
                    // Unregister a callback
                    {
                        std::lock_guard<std::mutex> lock(callback_ids_mutex);
                        if (!active_callback_ids.empty()) {
                            int id_to_remove = active_callback_ids.back();
                            active_callback_ids.pop_back();
                            pool_manager.unregisterMemoryPressureCallback(id_to_remove);
                            unregistrations++;
                        }
                    }
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    
                } catch (...) {
                    errors++;
                }
            }
        });
        
        // Thread that triggers operations that might call callbacks
        std::thread operation_thread([&]() {
            std::string component_name = "callback_test";
            
            while (test_running) {
                try {
                    // Perform operations that might trigger callbacks
                    uint8_t* buffer = pool_manager.allocateBuffer(64 * 1024, component_name);
                    if (buffer) {
                        pool_manager.releaseBuffer(buffer, 64 * 1024, component_name);
                    }
                    
                    pool_manager.optimizeMemoryUsage();
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    
                } catch (...) {
                    errors++;
                }
            }
        });
        
        // Let the test run
        std::this_thread::sleep_for(std::chrono::seconds(2));
        test_running = false;
        
        // Wait for threads to complete
        registration_thread.join();
        operation_thread.join();
        
        // Clean up remaining callbacks
        {
            std::lock_guard<std::mutex> lock(callback_ids_mutex);
            for (int callback_id : active_callback_ids) {
                try {
                    pool_manager.unregisterMemoryPressureCallback(callback_id);
                    unregistrations++;
                } catch (...) {
                    errors++;
                }
            }
        }
        
        // Verify results
        ASSERT_TRUE(registrations.load() > 0, "Should have registered callbacks");
        ASSERT_TRUE(unregistrations.load() > 0, "Should have unregistered callbacks");
        ASSERT_EQUALS(errors.load(), 0, "Should not have any errors");
        
        Debug::log("test", "Callback registration concurrency test completed - registrations: ", registrations.load(), 
                  ", unregistrations: ", unregistrations.load(), ", callback calls: ", callback_calls.load(), 
                  ", errors: ", errors.load());
    }
};

int main() {
    Debug::log("test", "Starting MemoryPoolManager comprehensive thread safety tests");
    
    MemoryPoolManagerThreadSafetyTest test;
    
    try {
        test.testConcurrentBufferOperations();
        test.testCallbackDeadlockPrevention();
        test.testMemoryPressureStress();
        test.testCallbackRegistrationConcurrency();
        
        Debug::log("test", "All MemoryPoolManager thread safety tests passed!");
        return 0;
        
    } catch (const std::exception& e) {
        Debug::log("test", "Test failed with exception: ", e.what());
        return 1;
    }
}