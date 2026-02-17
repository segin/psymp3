/*
 * test_memory_pool_allocation_failure.cpp - Test memory allocation failure handling in MemoryPoolManager
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <iostream>
#include <new>
#include <cstdlib>
#include <vector>

using PsyMP3::IO::MemoryPoolManager;

// Global flags to control memory allocation failure
bool g_simulate_bad_alloc = false;
size_t g_target_alloc_size = 0;

// Overload global operator new to simulate allocation failure
void* operator new(size_t size) {
    if (g_simulate_bad_alloc && size == g_target_alloc_size) {
        throw std::bad_alloc();
    }
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    std::free(ptr);
}

// Ensure array new/delete match
void* operator new[](size_t size) {
    return ::operator new(size);
}

void operator delete[](void* ptr) noexcept {
    ::operator delete(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    ::operator delete(ptr);
}

void test_pool_allocation_failure() {
    std::cout << "Testing pool allocation failure handling..." << std::endl;

    MemoryPoolManager& manager = MemoryPoolManager::getInstance();
    manager.initializePools();

    // Target a specific pool size
    const size_t pool_size = 64 * 1024;
    const std::string component = "test_failure";

    // 1. Identify the pool and drain it
    auto stats_initial = manager.getMemoryStats();

    // Find the pool index for 64KB
    int target_pool_idx = -1;
    for (int i = 0; i < 20; ++i) {
        std::string size_key = "pool_" + std::to_string(i) + "_size";
        if (stats_initial.count(size_key) && stats_initial[size_key] == pool_size) {
            target_pool_idx = i;
            break;
        }
    }

    if (target_pool_idx == -1) {
        std::cout << "Could not find pool for size " << pool_size << std::endl;
        exit(1);
    }

    std::string free_key = "pool_" + std::to_string(target_pool_idx) + "_free_buffers";
    size_t free_buffers = stats_initial[free_key];

    std::cout << "Draining pool of size " << pool_size << " (free buffers: " << free_buffers << ")..." << std::endl;
    std::vector<uint8_t*> allocated_buffers;

    for (size_t i = 0; i < free_buffers; ++i) {
        uint8_t* buf = manager.allocateBuffer(pool_size, component);
        if (buf) allocated_buffers.push_back(buf);
    }

    // Refresh stats to get current misses
    auto stats_before = manager.getMemoryStats();
    std::string misses_key = "pool_" + std::to_string(target_pool_idx) + "_misses";
    size_t misses_before = stats_before[misses_key];

    // 2. Setup failure simulation
    std::cout << "Simulating allocation failure for size " << pool_size << "..." << std::endl;
    g_target_alloc_size = pool_size;
    g_simulate_bad_alloc = true;

    // 3. Trigger allocation
    // This should try to create a new buffer for the pool, FAIL, catch exception,
    // increment misses, and then try direct allocation (which will also fail because size matches).

    // We expect catch block execution.

    uint8_t* result = manager.allocateBuffer(pool_size, component);

    g_simulate_bad_alloc = false;

    if (result == nullptr) {
        std::cout << "Allocation correctly returned nullptr after failure." << std::endl;
    } else {
        std::cout << "Allocation returned a buffer! Unexpected." << std::endl;
        // Clean up if it did return something
        manager.releaseBuffer(result, pool_size, component);
        exit(1);
    }

    // 4. Verify side effects (misses increased)
    auto stats_after = manager.getMemoryStats();
    size_t misses_after = stats_after[misses_key];

    if (misses_after > misses_before) {
         std::cout << "Verified misses incremented (" << misses_before << " -> " << misses_after << ")." << std::endl;
    } else {
         std::cout << "Misses did not increment! Catch block may not have been executed." << std::endl;
         exit(1);
    }

    // Clean up
    for (uint8_t* buf : allocated_buffers) {
        manager.releaseBuffer(buf, pool_size, component);
    }

    std::cout << "Test passed!" << std::endl;
}

int main() {
    try {
        test_pool_allocation_failure();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
