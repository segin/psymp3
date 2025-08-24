/*
 * test_memory_pool_manager_integration.cpp - Integration tests for MemoryPoolManager and MemoryTracker
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

class MemoryPoolManagerIntegrationTest {
public:
    void testMemoryTrackerIntegration() {
        Debug::log("test", "MemoryPoolManagerIntegrationTest::testMemoryTrackerIntegration() - Starting integration test");
        
        auto& pool_manager = MemoryPoolManager::getInstance();
        auto& memory_tracker = MemoryTracker::getInstance();
        
        // Initialize pools
        pool_manager.initializePools();
        
        // Start memory tracking
        memory_tracker.startAutoTracking(100); // Very frequent updates for testing
        
        std::atomic<bool> test_running{true};
        std::atomic<int> allocation_count{0};
        std::atomic<int> callback_count{0};
        std::atomic<int> deadlock_detected{0};
        
        // Register a callback to monitor pressure changes
        int callback_id = pool_manager.registerMemoryPressureCallback([&](int pressure) {
            callback_count++;
            Debug::log("test", "Pressure callback received: ", pressure);
            
            // Try to allocate memory from within callback to test for deadlocks
            try {
                uint8_t* buffer = pool_manager.allocateBuffer(1024, "callback_test");
                if (buffer) {
                    pool_manager.releaseBuffer(buffer, 1024, "callback_test");
                }
            } catch (...) {
                deadlock_detected++;
            }
        });
        
        // Create multiple threads that allocate and release buffers
        std::vector<std::thread> threads;
        const int num_threads = 4;
        
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&, i]() {
                std::vector<uint8_t*> buffers;
                
                while (test_running) {
                    try {
                        // Allocate some buffers
                        for (int j = 0; j < 10; j++) {
                            size_t size = (1024 << (j % 4)); // 1KB to 8KB
                            uint8_t* buffer = pool_manager.allocateBuffer(size, "thread_" + std::to_string(i));
                            if (buffer) {
                                buffers.push_back(buffer);
                                allocation_count++;
                            }
                        }
                        
                        // Release half of them
                        for (size_t k = 0; k < buffers.size() / 2; k++) {
                            size_t size = (1024 << (k % 4));
                            pool_manager.releaseBuffer(buffers[k], size, "thread_" + std::to_string(i));
                        }
                        buffers.erase(buffers.begin(), buffers.begin() + buffers.size() / 2);
                        
                        // Trigger memory optimization occasionally
                        if (allocation_count % 100 == 0) {
                            pool_manager.optimizeMemoryUsage();
                        }
                        
                        // Small delay to allow other threads to run
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        
                    } catch (...) {
                        deadlock_detected++;
                        break;
                    }
                }
                
                // Clean up remaining buffers
                for (size_t k = 0; k < buffers.size(); k++) {
                    size_t size = (1024 << (k % 4));
                    pool_manager.releaseBuffer(buffers[k], size, "thread_" + std::to_string(i));
                }
            });
        }
        
        // Let the test run for a short time
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Stop the test
        test_running = false;
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Stop memory tracking
        memory_tracker.stopAutoTracking();
        
        // Unregister callback
        pool_manager.unregisterMemoryPressureCallback(callback_id);
        
        // Verify results
        ASSERT_TRUE(allocation_count.load() > 0, "Should have performed allocations");
        ASSERT_TRUE(callback_count.load() > 0, "Should have received pressure callbacks");
        ASSERT_EQUALS(deadlock_detected.load(), 0, "Should not have detected any deadlocks");
        
        Debug::log("test", "Integration test completed - allocations: ", allocation_count.load(), 
                  ", callbacks: ", callback_count.load(), ", deadlocks: ", deadlock_detected.load());
    }
    
    void testCallbackReentrancyPrevention() {
        Debug::log("test", "MemoryPoolManagerIntegrationTest::testCallbackReentrancyPrevention() - Testing reentrancy prevention");
        
        auto& pool_manager = MemoryPoolManager::getInstance();
        
        std::atomic<int> callback_executions{0};
        std::atomic<int> reentrancy_attempts{0};
        std::atomic<bool> reentrancy_detected{false};
        
        // Register a callback that tries to trigger more callbacks
        int callback_id = pool_manager.registerMemoryPressureCallback([&](int pressure) {
            callback_executions++;
            
            // Try to trigger reentrancy by calling methods that might trigger callbacks
            try {
                pool_manager.optimizeMemoryUsage();
                reentrancy_attempts++;
            } catch (...) {
                reentrancy_detected = true;
            }
        });
        
        // Manually trigger callbacks multiple times rapidly
        for (int i = 0; i < 10; i++) {
            pool_manager.optimizeMemoryUsage();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Allow time for any queued callbacks to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Unregister callback
        pool_manager.unregisterMemoryPressureCallback(callback_id);
        
        // Verify that callbacks executed but reentrancy was handled safely
        ASSERT_TRUE(callback_executions.load() > 0, "Should have executed callbacks");
        ASSERT_FALSE(reentrancy_detected.load(), "Should not have detected reentrancy issues");
        
        Debug::log("test", "Reentrancy test completed - executions: ", callback_executions.load(), 
                  ", attempts: ", reentrancy_attempts.load(), ", detected: ", reentrancy_detected.load());
    }
    
    void testHighConcurrencyStress() {
        Debug::log("test", "MemoryPoolManagerIntegrationTest::testHighConcurrencyStress() - Starting stress test");
        
        auto& pool_manager = MemoryPoolManager::getInstance();
        auto& memory_tracker = MemoryTracker::getInstance();
        
        // Start aggressive memory tracking
        memory_tracker.startAutoTracking(50); // Very frequent updates
        
        std::atomic<bool> stress_running{true};
        std::atomic<int> total_operations{0};
        std::atomic<int> errors{0};
        
        // Create many threads performing various operations
        std::vector<std::thread> threads;
        const int num_threads = 8;
        
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&, i]() {
                std::vector<std::pair<uint8_t*, size_t>> allocated_buffers;
                
                while (stress_running) {
                    try {
                        int operation = total_operations++ % 10;
                        
                        switch (operation) {
                            case 0:
                            case 1:
                            case 2: {
                                // Allocate buffer
                                size_t size = 1024 * (1 + (i % 8));
                                uint8_t* buffer = pool_manager.allocateBuffer(size, "stress_" + std::to_string(i));
                                if (buffer) {
                                    allocated_buffers.push_back({buffer, size});
                                }
                                break;
                            }
                            case 3:
                            case 4: {
                                // Release buffer
                                if (!allocated_buffers.empty()) {
                                    auto buffer_info = allocated_buffers.back();
                                    allocated_buffers.pop_back();
                                    pool_manager.releaseBuffer(buffer_info.first, buffer_info.second, "stress_" + std::to_string(i));
                                }
                                break;
                            }
                            case 5: {
                                // Get memory stats
                                auto stats = pool_manager.getMemoryStats();
                                (void)stats; // Suppress unused variable warning
                                break;
                            }
                            case 6: {
                                // Check if safe to allocate
                                bool safe = pool_manager.isSafeToAllocate(64 * 1024, "stress_" + std::to_string(i));
                                (void)safe; // Suppress unused variable warning
                                break;
                            }
                            case 7: {
                                // Get optimal buffer size
                                size_t optimal = pool_manager.getOptimalBufferSize(32 * 1024, "stress_" + std::to_string(i));
                                (void)optimal; // Suppress unused variable warning
                                break;
                            }
                            case 8: {
                                // Optimize memory usage
                                pool_manager.optimizeMemoryUsage();
                                break;
                            }
                            case 9: {
                                // Register and immediately unregister callback
                                int id = pool_manager.registerMemoryPressureCallback([](int) {});
                                pool_manager.unregisterMemoryPressureCallback(id);
                                break;
                            }
                        }
                        
                    } catch (...) {
                        errors++;
                    }
                }
                
                // Clean up allocated buffers
                for (auto& buffer_info : allocated_buffers) {
                    try {
                        pool_manager.releaseBuffer(buffer_info.first, buffer_info.second, "stress_" + std::to_string(i));
                    } catch (...) {
                        errors++;
                    }
                }
            });
        }
        
        // Run stress test for a short time
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Stop the stress test
        stress_running = false;
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Stop memory tracking
        memory_tracker.stopAutoTracking();
        
        // Verify results
        ASSERT_TRUE(total_operations.load() > 1000, "Should have performed many operations");
        ASSERT_EQUALS(errors.load(), 0, "Should not have encountered any errors");
        
        Debug::log("test", "Stress test completed - operations: ", total_operations.load(), 
                  ", errors: ", errors.load());
    }
};

int main() {
    Debug::log("test", "Starting MemoryPoolManager integration tests");
    
    MemoryPoolManagerIntegrationTest test;
    
    try {
        test.testMemoryTrackerIntegration();
        test.testCallbackReentrancyPrevention();
        test.testHighConcurrencyStress();
        
        Debug::log("test", "All MemoryPoolManager integration tests passed!");
        return 0;
        
    } catch (const std::exception& e) {
        Debug::log("test", "Test failed with exception: ", e.what());
        return 1;
    }
}