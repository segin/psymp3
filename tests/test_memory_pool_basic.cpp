/*
 * test_memory_pool_basic.cpp - Basic test without pool initialization
 */

#include "psymp3.h"

int main() {
    std::cout << "Basic MemoryPoolManager test" << std::endl;
    
    try {
        MemoryPoolManager& manager = MemoryPoolManager::getInstance();
        std::cout << "Got MemoryPoolManager instance" << std::endl;
        
        // Don't initialize pools - just test direct allocation
        size_t test_size = 1024; // 1KB
        std::cout << "Testing allocation of " << test_size << " bytes" << std::endl;
        
        uint8_t* buffer = manager.allocateBuffer(test_size, "basic_test");
        
        if (buffer) {
            std::cout << "Got buffer at " << (void*)buffer << std::endl;
            
            // Try to write to it
            memset(buffer, 0xDD, test_size);
            std::cout << "Write successful" << std::endl;
            
            manager.releaseBuffer(buffer, test_size, "basic_test");
            std::cout << "Buffer released" << std::endl;
        } else {
            std::cout << "Allocation failed!" << std::endl;
            return 1;
        }
        
        std::cout << "Test completed successfully" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
        return 1;
    }
}