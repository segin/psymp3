/*
 * test_memory_pool_simple.cpp - Simple test for MemoryPoolManager allocation
 */

#include "psymp3.h"

int main() {
    std::cout << "Simple MemoryPoolManager allocation test" << std::endl;
    
    MemoryPoolManager& manager = MemoryPoolManager::getInstance();
    manager.initializePools();
    
    // Test allocation of 56KB - this should get a 64KB buffer from pool
    size_t requested_size = 56 * 1024; // 56KB
    std::cout << "Requesting " << requested_size << " bytes (56KB)" << std::endl;
    
    uint8_t* buffer = manager.allocateBuffer(requested_size, "test");
    
    if (buffer) {
        std::cout << "Got buffer: " << (void*)buffer << std::endl;
        
        // Try to write only the requested amount
        std::cout << "Writing " << requested_size << " bytes..." << std::endl;
        memset(buffer, 0xAA, requested_size);
        std::cout << "Write successful" << std::endl;
        
        // Get stats to see what happened
        auto stats = manager.getMemoryStats();
        std::cout << "Total allocated: " << stats["total_allocated"] << " bytes" << std::endl;
        
        // Release the buffer
        manager.releaseBuffer(buffer, requested_size, "test");
        std::cout << "Buffer released" << std::endl;
    } else {
        std::cout << "Allocation failed!" << std::endl;
        return 1;
    }
    
    return 0;
}