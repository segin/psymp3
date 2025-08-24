/*
 * test_memory_pool_minimal.cpp - Minimal test to isolate the buffer overflow
 */

#include "psymp3.h"

int main() {
    std::cout << "Minimal MemoryPoolManager test" << std::endl;
    
    MemoryPoolManager& manager = MemoryPoolManager::getInstance();
    manager.initializePools();
    
    // Test just one problematic allocation: 24KB
    size_t test_size = 24 * 1024; // 24KB
    std::cout << "Testing allocation of " << test_size << " bytes (24KB)" << std::endl;
    
    uint8_t* buffer = manager.allocateBuffer(test_size, "minimal_test");
    
    if (buffer) {
        std::cout << "Got buffer at " << (void*)buffer << std::endl;
        
        // Try to write exactly the requested amount
        std::cout << "Writing " << test_size << " bytes..." << std::endl;
        memset(buffer, 0xCC, test_size);
        std::cout << "Write successful" << std::endl;
        
        manager.releaseBuffer(buffer, test_size, "minimal_test");
        std::cout << "Buffer released" << std::endl;
    } else {
        std::cout << "Allocation failed!" << std::endl;
        return 1;
    }
    
    return 0;
}