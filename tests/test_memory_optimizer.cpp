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
    auto stats = queue.getStats();
    std::cout << "Queue stats: " << stats.current_items << " items, "
              << stats.current_memory_bytes << " bytes, "
              << stats.total_items_pushed << " pushed, "
              << stats.total_items_dropped << " dropped" << std::endl;
    
    // Pop items
    int value;
    while (queue.tryPop(value)) {
        std::cout << "Popped: " << value << std::endl;
    }
    
    // Get stats after pop
    stats = queue.getStats();
    std::cout << "Queue stats after pop: " << stats.current_items << " items, "
              << stats.current_memory_bytes << " bytes" << std::endl;
    
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
    std::cout << "  Memory pressure level: " << stats.memory_pressure_level << "%" << std::endl;
    
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
    
    std::cout << "All tests completed." << std::endl;
    
    return 0;
}