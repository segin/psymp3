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
    std::cout << "Queue stats: " << queue.size() << " items, "
              << queue.memoryUsage() << " bytes" << std::endl;
    
    // Pop items
    int value;
    while (queue.tryPop(value)) {
        std::cout << "Popped: " << value << std::endl;
    }
    
    // Get stats after pop
    std::cout << "Queue stats after pop: " << queue.size() << " items, "
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

// Test the MemoryOptimizer
void testMemoryOptimizer() {
    std::cout << "Testing MemoryOptimizer..." << std::endl;

    MemoryOptimizer& optimizer = MemoryOptimizer::getInstance();

    // Test initial state
    auto stats = optimizer.getMemoryStats();
    std::cout << "Initial memory stats: " << stats["total_memory_usage"] << " bytes used" << std::endl;

    // Set memory limits
    size_t test_max_total = 10 * 1024 * 1024; // 10MB
    size_t test_max_buffer = 5 * 1024 * 1024; // 5MB
    optimizer.setMemoryLimits(test_max_total, test_max_buffer);

    stats = optimizer.getMemoryStats();
    std::cout << "After setting limits - Max total: " << stats["max_total_memory"]
              << ", Max buffer: " << stats["max_buffer_memory"] << std::endl;

    // Test safe to allocate
    bool is_safe = optimizer.isSafeToAllocate(1 * 1024 * 1024, "audio"); // 1MB
    std::cout << "Is 1MB audio allocation safe? " << (is_safe ? "Yes" : "No") << std::endl;

    is_safe = optimizer.isSafeToAllocate(6 * 1024 * 1024, "buffer"); // 6MB (exceeds buffer limit)
    std::cout << "Is 6MB buffer allocation safe? " << (is_safe ? "Yes" : "No") << std::endl;

    // Register allocations
    std::cout << "Registering allocations..." << std::endl;
    optimizer.registerAllocation(2 * 1024 * 1024, "audio"); // 2MB
    optimizer.registerAllocation(3 * 1024 * 1024, "buffer"); // 3MB

    stats = optimizer.getMemoryStats();
    std::cout << "After allocations - Total used: " << stats["total_memory_usage"]
              << ", Audio used: " << stats["component_audio"]
              << ", Buffer used: " << stats["component_buffer"] << std::endl;

    // Test cache trimming
    std::cout << "Testing cache trimming behavior..." << std::endl;

    IOBufferPool& buffer_pool = IOBufferPool::getInstance();

    // Check initial state
    auto pool_stats = buffer_pool.getStats();
    size_t initial_max_pool = pool_stats["max_pool_size"];
    size_t initial_max_buffers = pool_stats["max_buffers_per_size"];
    std::cout << "Initial BufferPool stats - Max size: " << initial_max_pool
              << ", Max buffers per size: " << initial_max_buffers << std::endl;

    // Reduce limits
    optimizer.setMemoryLimits(4 * 1024 * 1024, 2 * 1024 * 1024); // Set limit to 4MB total

    // Call optimizeMemoryUsage manually. Even at Normal pressure, it sets IOBufferPool limits
    // to 25% of total max memory, which is 1MB. This should be much less than the initial 16MB default.
    optimizer.optimizeMemoryUsage();

    // Verify that BufferPool limits have been reduced by optimizeMemoryUsage
    pool_stats = buffer_pool.getStats();
    size_t new_max_pool = pool_stats["max_pool_size"];
    size_t new_max_buffers = pool_stats["max_buffers_per_size"];
    std::cout << "New BufferPool stats - Max size: " << new_max_pool
              << ", Max buffers per size: " << new_max_buffers << std::endl;

    if (new_max_pool >= initial_max_pool || new_max_buffers >= initial_max_buffers) {
        std::cerr << "ASSERTION FAILED: IOBufferPool limits were not trimmed as expected." << std::endl;
        std::exit(1);
    }

    // Get recommended buffer pool params based on current pressure level
    size_t recommended_pool_size, recommended_buffers_per_size;
    optimizer.getRecommendedBufferPoolParams(recommended_pool_size, recommended_buffers_per_size);
    std::cout << "Recommended buffer pool params - Max size: " << recommended_pool_size
              << ", Max buffers per size: " << recommended_buffers_per_size << std::endl;

    // Test read ahead recommendations
    bool should_read_ahead = optimizer.shouldEnableReadAhead();
    size_t read_ahead_size = optimizer.getRecommendedReadAheadSize(1024 * 1024);
    std::cout << "Should enable read ahead? " << (should_read_ahead ? "Yes" : "No")
              << ", Recommended size: " << read_ahead_size << std::endl;

    // Register deallocations
    std::cout << "Registering deallocations..." << std::endl;
    optimizer.registerDeallocation(1 * 1024 * 1024, "audio"); // 1MB
    optimizer.registerDeallocation(3 * 1024 * 1024, "buffer"); // 3MB

    stats = optimizer.getMemoryStats();
    std::cout << "After deallocations - Total used: " << stats["total_memory_usage"]
              << ", Audio used: " << stats["component_audio"]
              << ", Buffer used: " << stats["component_buffer"] << std::endl;

    std::cout << "MemoryOptimizer test completed." << std::endl;
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
    
    testMemoryOptimizer();
    std::cout << std::endl;

    std::cout << "All tests completed." << std::endl;
    
    return 0;
}