/*
 * test_memory_pool_manager_basic_threading.cpp - Basic threading test for MemoryPoolManager
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>

/**
 * Very basic test to verify MemoryPoolManager works in a threaded environment
 */
int main() {
    std::cout << "Basic MemoryPoolManager threading test" << std::endl;
    
    try {
        MemoryPoolManager& manager = MemoryPoolManager::getInstance();
        std::cout << "Got MemoryPoolManager instance" << std::endl;
        
        manager.initializePools();
        std::cout << "Initialized pools" << std::endl;
        
        std::atomic<int> successful_operations{0};
        std::atomic<int> errors{0};
        
        const int num_threads = 2;
        const int operations_per_thread = 10;
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&, i]() {
                std::string component_name = "test_" + std::to_string(i);
                
                for (int j = 0; j < operations_per_thread; j++) {
                    try {
                        // Simple allocation and immediate release
                        uint8_t* buffer = manager.allocateBuffer(4096, component_name);
                        if (buffer) {
                            buffer[0] = 0xAA; // Write to verify buffer is valid
                            manager.releaseBuffer(buffer, 4096, component_name);
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
        
        // Wait for threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Test completed:" << std::endl;
        std::cout << "  Successful operations: " << successful_operations.load() << std::endl;
        std::cout << "  Errors: " << errors.load() << std::endl;
        
        if (errors.load() == 0 && successful_operations.load() > 0) {
            std::cout << "✓ Basic threading test PASSED" << std::endl;
            return 0;
        } else {
            std::cout << "✗ Basic threading test FAILED" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}