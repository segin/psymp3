/*
 * test_memory_pool_allocation_failure.cpp - Test for MemoryPoolManager allocation failure handling
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <iostream>
#include <new>
#include <atomic>
#include <vector>
#include <cstdlib>
#include <string>
#include <map>

// Global flags to control allocation failure
std::atomic<bool> g_should_fail_allocation{false};
std::atomic<size_t> g_fail_size{0};

// Custom operator new[] to simulate failure
void* operator new[](std::size_t size) {
    if (g_should_fail_allocation && size == g_fail_size) {
        throw std::bad_alloc();
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

// We also need to override scalar new/delete to avoid linker errors about missing symbols if we only override array new
void* operator new(std::size_t size) {
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    std::free(p);
}

// Ensure scalar delete(void*, size_t) is also defined for C++14 compliance
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}

using namespace PsyMP3::IO;

void test_pool_allocation_failure() {
    std::cout << "Testing pool allocation failure handling..." << std::endl;

    // Get the singleton
    auto& manager = MemoryPoolManager::getInstance();
    manager.initializePools();

    // Target pool size: 4KB (4096 bytes)
    const size_t TARGET_SIZE = 4096;
    const std::string COMPONENT = "test_failure";

    // Step 1: Drain the pool dynamically
    std::vector<uint8_t*> initial_buffers;

    // Helper lambda to get free buffers count
    auto get_free_buffers = [&]() -> size_t {
        auto stats = manager.getMemoryStats();
        for (const auto& pair : stats) {
            if (pair.first.find("pool_") == 0 && pair.first.find("_size") != std::string::npos) {
                if (pair.second == TARGET_SIZE) {
                    std::string prefix = pair.first.substr(0, pair.first.find("_size") + 1);
                    return stats[prefix + "free_buffers"];
                }
            }
        }
        return 0; // Should not happen if pool exists
    };

    size_t free_bufs = get_free_buffers();
    std::cout << "Initial free buffers: " << free_bufs << std::endl;

    while (free_bufs > 0) {
        uint8_t* buf = manager.allocateBuffer(TARGET_SIZE, COMPONENT);
        if (buf) {
            initial_buffers.push_back(buf);
        } else {
            std::cerr << "Failed to allocate initial buffer while draining pool" << std::endl;
            exit(1);
        }
        free_bufs = get_free_buffers();
    }

    std::cout << "Allocated " << initial_buffers.size() << " initial buffers. Pool should be empty of free buffers." << std::endl;

    // Step 2: Trigger allocation failure
    // The next allocation will try to create a new buffer because allocated_buffers < max_buffers (assuming max > pre-allocated).
    // Note: If pre-allocated == max_buffers, then this test won't trigger the specific path (it will go to direct allocation immediately).
    // But default config is 16 max, 4 pre-allocated.

    g_fail_size = TARGET_SIZE;
    g_should_fail_allocation = true;

    std::cout << "Triggering allocation failure for size " << TARGET_SIZE << "..." << std::endl;

    uint8_t* fallback_buffer = nullptr;
    try {
        // This should trigger the failure in the pool logic, catch it, log it,
        // and fall back to direct allocation (which will also fail because our mock fails for that size).
        fallback_buffer = manager.allocateBuffer(TARGET_SIZE, COMPONENT);

    } catch (const std::exception& e) {
        std::cerr << "Unexpected exception during test: " << e.what() << std::endl;
    }

    g_should_fail_allocation = false;

    if (fallback_buffer == nullptr) {
        std::cout << "Allocation returned nullptr as expected (both pool and direct allocation failed)." << std::endl;
    } else {
        std::cout << "Allocation succeeded unexpectedly." << std::endl;
    }

    // Step 3: Verify stats
    auto stats = manager.getMemoryStats();

    // We need to find the pool index for 4096.
    bool found_pool = false;
    size_t misses = 0;

    for (const auto& pair : stats) {
        if (pair.first.find("pool_") == 0 && pair.first.find("_size") != std::string::npos) {
            if (pair.second == TARGET_SIZE) {
                // Found the pool. Get the prefix.
                std::string prefix = pair.first.substr(0, pair.first.find("_size") + 1); // "pool_X_"
                std::string misses_key = prefix + "misses";
                if (stats.count(misses_key)) {
                    misses = stats[misses_key];
                    found_pool = true;
                    std::cout << "Pool " << prefix << " misses: " << misses << std::endl;
                }
                break;
            }
        }
    }

    if (!found_pool) {
        std::cerr << "Could not find pool for size " << TARGET_SIZE << std::endl;
        exit(1);
    }

    if (misses > 0) {
        std::cout << "✓ SUCCESS: Pool misses incremented, indicating the bad_alloc catch block was executed." << std::endl;
    } else {
        std::cerr << "✗ FAILURE: Pool misses did not increment." << std::endl;
        exit(1);
    }

    // Cleanup
    for (auto* buf : initial_buffers) {
        manager.releaseBuffer(buf, TARGET_SIZE, COMPONENT);
    }
}

int main() {
    try {
        test_pool_allocation_failure();
        return 0;
    } catch (...) {
        return 1;
    }
}
