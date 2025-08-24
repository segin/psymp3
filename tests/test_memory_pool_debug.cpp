/*
 * test_memory_pool_debug.cpp - Debug version to isolate the buffer overflow issue
 */

#include "psymp3.h"
#include <thread>
#include <vector>

int main() {
    std::cout << "Debug MemoryPoolManager test" << std::endl;
    
    MemoryPoolManager& manager = MemoryPoolManager::getInstance();
    manager.initializePools();
    
    // Test the specific case that's failing: 48KB allocation
    size_t test_size = 48 * 1024; // 48KB
    std::cout << "Testing allocation of " << test_size << " bytes" << std::endl;
    
    // Single-threaded test first
    std::cout << "Single-threaded test:" << std::endl;
    uint8_t* buffer1 = manager.allocateBuffer(test_size, "debug_test");
    if (buffer1) {
        std::cout << "  Allocated buffer at " << (void*)buffer1 << std::endl;
        
        // Try to write to it
        try {
            memset(buffer1, 0xAA, test_size);
            std::cout << "  Write successful" << std::endl;
        } catch (...) {
            std::cout << "  Write failed!" << std::endl;
        }
        
        manager.releaseBuffer(buffer1, test_size, "debug_test");
        std::cout << "  Buffer released" << std::endl;
    } else {
        std::cout << "  Allocation failed!" << std::endl;
    }
    
    // Multi-threaded test with 8 threads like original
    std::cout << "\nMulti-threaded test (8 threads):" << std::endl;
    
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};
    
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&, i]() {
            std::string component_name = "debug_thread_" + std::to_string(i);
            std::vector<std::pair<uint8_t*, size_t>> allocated_buffers;
            
            for (int j = 0; j < 100; ++j) {
                try {
                    // Use the same size pattern as the original test
                    size_t requested_size = (j % 8 + 1) * 8 * 1024; // 8KB to 64KB
                    uint8_t* buffer = manager.allocateBuffer(requested_size, component_name);
                    
                    if (buffer) {
                        allocated_buffers.push_back({buffer, requested_size});
                        
                        printf("Thread %d iteration %d: allocated %zu bytes at %p\n", 
                               i, j, requested_size, (void*)buffer);
                        
                        // Try to write to it
                        memset(buffer, 0xBB, requested_size);
                        printf("Thread %d iteration %d: write successful\n", i, j);
                        
                        // Occasionally release some buffers (like in original test)
                        if (allocated_buffers.size() > 10) {
                            auto& to_release = allocated_buffers.back();
                            manager.releaseBuffer(to_release.first, to_release.second, component_name);
                            printf("Thread %d: released buffer of size %zu\n", i, to_release.second);
                            allocated_buffers.pop_back();
                        }
                    } else {
                        printf("Thread %d iteration %d: allocation failed\n", i, j);
                        errors++;
                    }
                } catch (...) {
                    printf("Thread %d iteration %d: exception occurred\n", i, j);
                    errors++;
                }
            }
            
            // Release all remaining buffers
            for (auto& buffer_info : allocated_buffers) {
                try {
                    manager.releaseBuffer(buffer_info.first, buffer_info.second, component_name);
                    printf("Thread %d: final release of buffer size %zu\n", i, buffer_info.second);
                } catch (...) {
                    printf("Thread %d: exception during final release\n", i);
                    errors++;
                }
            }
        });
    }
    
    // Wait for threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Multi-threaded test completed with " << errors.load() << " errors" << std::endl;
    
    return errors.load() > 0 ? 1 : 0;
}