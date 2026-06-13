#include "psymp3.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cstdlib>
#include <ctime>

// Test the EnhancedBufferPool
void testEnhancedBufferPool() {
    std::cout << "Testing EnhancedBufferPool..." << std::endl;
    
    EnhancedBufferPool& pool = EnhancedBufferPool::getInstance();
    
    // Get initial stats
    auto initial_stats = pool.getStats();
    std::cout << "Initial stats: " << initial_stats.total_buffers << " buffers, "
              << initial_stats.total_memory_bytes << " bytes" << std::endl;
    
    // Test buffer allocation and reuse
    std::vector<std::vector<uint8_t>> buffers;
    
    // Allocate buffers of various sizes
    for (int i = 0; i < 100; i++) {
        size_t size = 1024 * (1 + (i % 10));
        buffers.push_back(pool.getBuffer(size));
        
        // Fill buffer with test data
        for (size_t j = 0; j < size && j < buffers.back().capacity(); j++) {
            buffers.back().push_back(static_cast<uint8_t>(j & 0xFF));
        }
    }
    
    // Get stats after allocation
    auto after_alloc_stats = pool.getStats();
    std::cout << "After allocation: " << after_alloc_stats.buffer_hits << " hits, "
              << after_alloc_stats.buffer_misses << " misses" << std::endl;
    
    // Return buffers to pool
    for (auto& buffer : buffers) {
        pool.returnBuffer(std::move(buffer));
    }
    buffers.clear();
    
    // Get stats after return
    auto after_return_stats = pool.getStats();
    std::cout << "After return: " << after_return_stats.total_buffers << " buffers, "
              << after_return_stats.total_memory_bytes << " bytes" << std::endl;
    
    // Test reuse
    for (int i = 0; i < 50; i++) {
        size_t size = 1024 * (1 + (i % 10));
        buffers.push_back(pool.getBuffer(size));
    }
    
    // Get stats after reuse
    auto after_reuse_stats = pool.getStats();
    std::cout << "After reuse: " << after_reuse_stats.buffer_hits << " hits, "
              << after_reuse_stats.buffer_misses << " misses" << std::endl;
    std::cout << "Hit ratio: " << (after_reuse_stats.buffer_hits * 100.0f) / 
                                  (after_reuse_stats.buffer_hits + after_reuse_stats.buffer_misses) << "%" << std::endl;
    
    // Clean up
    buffers.clear();
    pool.clear();
    
    std::cout << "EnhancedBufferPool test completed." << std::endl;
}

// Test the BoundedQueue
void testBoundedQueue() {
    std::cout << "Testing BoundedQueue..." << std::endl;
    
    // Create a bounded queue for integers
    auto memory_estimator = [](const int& value) -> size_t {
        return sizeof(int);
    };
    
    BoundedQueue<int> queue(10, 1000, memory_estimator);
    
    // Push items
    for (int i = 0; i < 15; i++) {
        bool result = queue.tryPush(i);
        std::cout << "Push " << i << ": " << (result ? "success" : "failed") << std::endl;
    }
    
    // Get stats
    std::cout << "Queue size: " << queue.size() << " items, "
              << queue.memoryUsage() << " bytes" << std::endl;
    
    // Pop items
    int value;
    while (queue.tryPop(value)) {
        std::cout << "Popped: " << value << std::endl;
    }
    
    // Get stats after pop
    std::cout << "Queue size after pop: " << queue.size() << " items, "
              << queue.memoryUsage() << " bytes" << std::endl;
    
    std::cout << "BoundedQueue test completed." << std::endl;
}

// Test the MemoryTracker
void testMemoryTracker() {
    std::cout << "Testing MemoryTracker..." << std::endl;
    
    MemoryTracker& tracker = MemoryTracker::getInstance();
    
    // Register a callback
    int callback_id = tracker.registerMemoryPressureCallback([](int pressure) {
        std::cout << "Memory pressure callback: " << pressure << "%" << std::endl;
    });
    
    // Update and get stats
    tracker.update();
    auto stats = tracker.getStats();
    
    std::cout << "Memory stats: " << std::endl;
    std::cout << "  Total physical memory: " << (stats.total_physical_memory / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  Available physical memory: " << (stats.available_physical_memory / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  Process memory usage: " << (stats.process_memory_usage / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  Memory pressure level: " << tracker.getMemoryPressureLevel() << "%" << std::endl;
    
    // Allocate some memory to change pressure
    std::vector<std::vector<uint8_t>> memory_blocks;
    for (int i = 0; i < 10; i++) {
        std::vector<uint8_t> block(1024 * 1024, 0); // 1MB block
        memory_blocks.push_back(std::move(block));
        
        // Update and print pressure
        tracker.update();
        std::cout << "After allocating " << (i + 1) << "MB: " 
                  << tracker.getMemoryPressureLevel() << "%" << std::endl;
    }
    
    // Clean up
    memory_blocks.clear();
    tracker.unregisterMemoryPressureCallback(callback_id);
    
    std::cout << "MemoryTracker test completed." << std::endl;
}

// Test the MemoryOptimizer Core functionality
void testMemoryOptimizerCore() {
    std::cout << "Testing MemoryOptimizer Core functionality..." << std::endl;

    MemoryOptimizer& optimizer = MemoryOptimizer::getInstance();
    IOBufferPool& pool = IOBufferPool::getInstance();

    // Initial state
    pool.clear();
    std::cout << "Initial pool size: " << pool.getStats().at("current_pool_size") << " bytes" << std::endl;

    // Set memory limits for MemoryOptimizer
    optimizer.setMemoryLimits(1024 * 1024, 512 * 1024); // 1MB total, 512KB buffer

    // Register some allocations to simulate usage
    optimizer.registerAllocation(500 * 1024, "test_component1");
    optimizer.registerAllocation(200 * 1024, "test_component2");

    auto mem_stats = optimizer.getMemoryStats();
    std::cout << "MemoryOptimizer total usage: " << mem_stats["total_memory_usage"] << " bytes" << std::endl;

    // Populate the IOBufferPool
    std::vector<IOBufferPool::Buffer> buffers;
    for (int i = 0; i < 50; i++) {
        buffers.push_back(pool.acquire(8192)); // 8KB buffers
    }

    // Release them back to populate the pool
    for (auto& buffer : buffers) {
        buffer.release();
    }
    buffers.clear();

    std::cout << "Pool size before optimization: " << pool.getStats().at("current_pool_size") << " bytes" << std::endl;

    // Set a critical memory pressure level
    // Memory usage is 716800 bytes, total limit is 1MB. (716KB / 1024KB = 70%)
    // Let's force optimization manually or change memory limits to trigger pressure
    optimizer.setMemoryLimits(700 * 1024, 512 * 1024);

    // Call optimizeMemoryUsage
    optimizer.optimizeMemoryUsage();

    std::cout << "Pool size after optimization: " << pool.getStats().at("current_pool_size") << " bytes" << std::endl;

    // Assert cache trimming
    if (pool.getStats().at("current_pool_size") >= 65536) {
        std::cerr << "Assertion failed: Pool size was not trimmed during optimization!" << std::endl;
        std::exit(1);
    }

    // Clean up
    pool.clear();
    optimizer.registerDeallocation(500 * 1024, "test_component1");
    optimizer.registerDeallocation(200 * 1024, "test_component2");

    std::cout << "MemoryOptimizer Core test completed." << std::endl;
}

// Main test function
int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    std::cout << "Memory Optimizer Tests" << std::endl;
    std::cout << "======================" << std::endl;
    
    testEnhancedBufferPool();
    std::cout << std::endl;
    
    testBoundedQueue();
    std::cout << std::endl;
    
    testMemoryTracker();
    std::cout << std::endl;
    
    testMemoryOptimizerCore();
    std::cout << std::endl;

    std::cout << "All tests completed." << std::endl;
    
    return 0;
}