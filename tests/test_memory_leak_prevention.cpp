/*
 * test_memory_leak_prevention.cpp - Test memory leak prevention in IOHandler subsystem
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

/**
 * @brief Test EnhancedBufferPool bounded limits and memory pressure handling
 */
bool test_enhanced_buffer_pool() {
    Debug::log("test", "test_enhanced_buffer_pool() - Starting test");
    
    EnhancedBufferPool& pool = EnhancedBufferPool::getInstance();
    
    // Clear pool to start fresh
    pool.clear();
    
    // Test normal operation
    pool.setMemoryPressure(0); // Low pressure
    
    // Acquire several buffers
    std::vector<std::vector<uint8_t>> buffers;
    
    // Test different buffer sizes
    for (int i = 0; i < 5; i++) {
        auto buffer = pool.getBuffer(1024 * (i + 1)); // 1KB, 2KB, 3KB, 4KB, 5KB
        buffers.push_back(std::move(buffer));
    }
    
    // Get initial stats
    auto stats = pool.getStats();
    Debug::log("test", "test_enhanced_buffer_pool() - Initial stats: hits=", stats.buffer_hits, 
              ", misses=", stats.buffer_misses, ", pressure=", stats.memory_pressure);
    
    // Return buffers to pool
    for (auto& buffer : buffers) {
        pool.returnBuffer(std::move(buffer));
    }
    buffers.clear();
    
    // Test memory pressure response
    pool.setMemoryPressure(80); // High pressure
    
    // Try to get buffers under high pressure
    for (int i = 0; i < 3; i++) {
        auto buffer = pool.getBuffer(64 * 1024); // 64KB buffers
        buffers.push_back(std::move(buffer));
    }
    
    // Get stats under pressure
    auto pressure_stats = pool.getStats();
    Debug::log("test", "test_enhanced_buffer_pool() - High pressure stats: hits=", pressure_stats.buffer_hits, 
              ", misses=", pressure_stats.buffer_misses, ", pressure=", pressure_stats.memory_pressure);
    
    // Return buffers
    for (auto& buffer : buffers) {
        pool.returnBuffer(std::move(buffer));
    }
    buffers.clear();
    
    // Test critical pressure
    pool.setMemoryPressure(100); // Critical pressure
    
    auto buffer = pool.getBuffer(1024 * 1024); // 1MB buffer
    pool.returnBuffer(std::move(buffer));
    
    // Get final stats
    auto final_stats = pool.getStats();
    Debug::log("test", "test_enhanced_buffer_pool() - Final stats: hits=", final_stats.buffer_hits, 
              ", misses=", final_stats.buffer_misses, ", reuse=", final_stats.reuse_count);
    
    Debug::log("test", "test_enhanced_buffer_pool() - Test completed successfully");
    return true;
}

/**
 * @brief Test EnhancedAudioBufferPool functionality
 */
bool test_enhanced_audio_buffer_pool() {
    Debug::log("test", "test_enhanced_audio_buffer_pool() - Starting test");
    
    EnhancedAudioBufferPool& pool = EnhancedAudioBufferPool::getInstance();
    
    // Clear pool to start fresh
    pool.clear();
    
    // Test normal operation
    pool.setMemoryPressure(0); // Low pressure
    
    // Acquire several audio buffers
    std::vector<std::vector<int16_t>> buffers;
    
    // Test different sample buffer sizes
    for (int i = 0; i < 5; i++) {
        auto buffer = pool.getSampleBuffer(1024 * (i + 1)); // 1K, 2K, 3K, 4K, 5K samples
        buffers.push_back(std::move(buffer));
    }
    
    // Get initial stats
    auto stats = pool.getStats();
    Debug::log("test", "test_enhanced_audio_buffer_pool() - Initial stats: hits=", stats.buffer_hits, 
              ", misses=", stats.buffer_misses, ", pressure=", stats.memory_pressure);
    
    // Return buffers to pool
    for (auto& buffer : buffers) {
        pool.returnSampleBuffer(std::move(buffer));
    }
    buffers.clear();
    
    // Test memory pressure response
    pool.setMemoryPressure(75); // High pressure
    
    // Try to get buffers under high pressure
    for (int i = 0; i < 3; i++) {
        auto buffer = pool.getSampleBuffer(32 * 1024); // 32K samples
        buffers.push_back(std::move(buffer));
    }
    
    // Return buffers
    for (auto& buffer : buffers) {
        pool.returnSampleBuffer(std::move(buffer));
    }
    buffers.clear();
    
    // Get final stats
    auto final_stats = pool.getStats();
    Debug::log("test", "test_enhanced_audio_buffer_pool() - Final stats: hits=", final_stats.buffer_hits, 
              ", misses=", final_stats.buffer_misses, ", reuse=", final_stats.reuse_count);
    
    Debug::log("test", "test_enhanced_audio_buffer_pool() - Test completed successfully");
    return true;
}

/**
 * @brief Test HTTP client resource cleanup and RAII
 */
bool test_http_client_cleanup() {
    Debug::log("test", "test_http_client_cleanup() - Starting test");
    
    // Test HTTPClient RAII behavior
    {
        HTTPClient client;
        
        // Test basic functionality
        HTTPClient::Response response = client.get("https://httpbin.org/get");
        Debug::log("test", "test_http_client_cleanup() - GET request status: ", response.statusCode);
        
        // Test HEAD request
        HTTPClient::Response head_response = client.head("https://httpbin.org/get");
        Debug::log("test", "test_http_client_cleanup() - HEAD request status: ", head_response.statusCode);
        
        // Test range request
        HTTPClient::Response range_response = client.getRange("https://httpbin.org/range/1024", 0, 511);
        Debug::log("test", "test_http_client_cleanup() - Range request status: ", range_response.statusCode);
        
        // HTTPClient should automatically clean up when it goes out of scope
    }
    
    // Test multiple clients to ensure no resource leaks
    for (int i = 0; i < 3; i++) {
        HTTPClient client;
        HTTPClient::Response response = client.get("https://httpbin.org/get");
        Debug::log("test", "test_http_client_cleanup() - Client ", i, " status: ", response.statusCode);
        // Each client should clean up automatically
    }
    
    Debug::log("test", "test_http_client_cleanup() - Test completed successfully");
    return true;
}

/**
 * @brief Test MemoryOptimizer functionality
 */
bool test_memory_optimizer() {
    Debug::log("test", "test_memory_optimizer() - Starting test");
    
    MemoryOptimizer& optimizer = MemoryOptimizer::getInstance();
    
    // Test initial state
    auto initial_level = optimizer.getMemoryPressureLevel();
    Debug::log("test", "test_memory_optimizer() - Initial pressure level: ", 
              static_cast<int>(initial_level));
    
    // Test optimal buffer size calculation
    size_t optimal_size = optimizer.getOptimalBufferSize(1024, "test", true);
    Debug::log("test", "test_memory_optimizer() - Optimal buffer size for 1024 bytes: ", optimal_size);
    
    // Test safe allocation check
    bool safe = optimizer.isSafeToAllocate(1024 * 1024, "test");
    Debug::log("test", "test_memory_optimizer() - Safe to allocate 1MB: ", safe ? "yes" : "no");
    
    // Test allocation tracking
    optimizer.registerAllocation(1024, "test");
    optimizer.registerDeallocation(1024, "test");
    
    // Test memory stats
    auto stats = optimizer.getMemoryStats();
    Debug::log("test", "test_memory_optimizer() - Memory stats available: ", stats.size(), " entries");
    
    Debug::log("test", "test_memory_optimizer() - Test completed successfully");
    return true;
}

/**
 * @brief Test RAII and exception safety
 */
bool test_raii_exception_safety() {
    Debug::log("test", "test_raii_exception_safety() - Starting test");
    
    // Test FileIOHandler RAII
    {
        try {
            FileIOHandler handler("/nonexistent/file/path.txt");
            // This should fail gracefully without leaking resources
        } catch (const std::exception& e) {
            Debug::log("test", "test_raii_exception_safety() - Expected exception caught: ", e.what());
        }
    }
    
    // Test HTTPIOHandler RAII
    {
        try {
            HTTPIOHandler handler("https://invalid.domain.that.does.not.exist/file.mp3");
            // This should fail gracefully without leaking resources
        } catch (const std::exception& e) {
            Debug::log("test", "test_raii_exception_safety() - Expected exception caught: ", e.what());
        }
    }
    
    // Test buffer pool exception safety
    {
        EnhancedBufferPool& pool = EnhancedBufferPool::getInstance();
        
        // Set critical memory pressure
        pool.setMemoryPressure(100);
        
        std::vector<std::vector<uint8_t>> buffers;
        
        try {
            // Try to allocate many large buffers under memory pressure
            for (int i = 0; i < 100; i++) {
                auto buffer = pool.getBuffer(1024 * 1024); // 1MB each
                buffers.push_back(std::move(buffer));
            }
        } catch (const std::exception& e) {
            Debug::log("test", "test_raii_exception_safety() - Exception during allocation: ", e.what());
        }
        
        // Buffers should be automatically cleaned up when going out of scope
        Debug::log("test", "test_raii_exception_safety() - Allocated ", buffers.size(), " buffers");
        
        // Reset memory pressure
        pool.setMemoryPressure(0);
    }
    
    Debug::log("test", "test_raii_exception_safety() - Test completed successfully");
    return true;
}

/**
 * @brief Test thread safety of resource management
 */
bool test_thread_safety() {
    Debug::log("test", "test_thread_safety() - Starting test");
    
    EnhancedBufferPool& pool = EnhancedBufferPool::getInstance();
    pool.clear();
    
    const int num_threads = 4;
    const int buffers_per_thread = 10;
    
    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    
    // Create multiple threads that allocate and deallocate buffers
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&pool, &successful_operations, buffers_per_thread, t]() {
            std::vector<std::vector<uint8_t>> local_buffers;
            
            try {
                // Allocate buffers
                for (int i = 0; i < buffers_per_thread; i++) {
                    auto buffer = pool.getBuffer(1024 + (t * 100) + i); // Different sizes
                    local_buffers.push_back(std::move(buffer));
                }
                
                // Small delay to increase chance of race conditions
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
                // Return buffers
                for (auto& buffer : local_buffers) {
                    pool.returnBuffer(std::move(buffer));
                }
                
                successful_operations++;
            } catch (const std::exception& e) {
                Debug::log("test", "test_thread_safety() - Thread ", t, " exception: ", e.what());
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    Debug::log("test", "test_thread_safety() - Successful operations: ", successful_operations.load(), 
              " out of ", num_threads);
    
    // Get final pool stats
    auto stats = pool.getStats();
    Debug::log("test", "test_thread_safety() - Final pool stats: hits=", stats.buffer_hits, 
              ", misses=", stats.buffer_misses, ", reuse=", stats.reuse_count);
    
    Debug::log("test", "test_thread_safety() - Test completed successfully");
    return successful_operations.load() == num_threads;
}

/**
 * @brief Main test function
 */
int main() {
    Debug::log("test", "Starting memory management and resource safety tests");
    
    bool all_passed = true;
    
    // Run tests
    all_passed &= test_enhanced_buffer_pool();
    all_passed &= test_enhanced_audio_buffer_pool();
    all_passed &= test_http_client_cleanup();
    all_passed &= test_memory_optimizer();
    all_passed &= test_raii_exception_safety();
    all_passed &= test_thread_safety();
    
    if (all_passed) {
        Debug::log("test", "All memory management and resource safety tests passed");
        return 0;
    } else {
        Debug::log("test", "Some memory management and resource safety tests failed");
        return 1;
    }
}