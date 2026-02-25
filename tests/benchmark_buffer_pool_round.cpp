#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <cstdint>

// Baseline implementation (Loop)
size_t roundToPoolSize_Baseline(size_t size) {
    if (size <= 1024 * 1024) {
        size_t rounded = 1;
        while (rounded < size) {
            rounded <<= 1;
        }
        return rounded;
    }
    const size_t alignment = 64 * 1024;
    return ((size + alignment - 1) / alignment) * alignment;
}

// Optimization 1: Portable Bit Twiddling
size_t roundToPoolSize_BitTrick(size_t size) {
    if (size <= 1024 * 1024) {
        if (size <= 1) return 1;
        size_t n = size - 1;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        // Since max is 1MB (20 bits), 16+8... covers it.
        // We don't strictly need >> 32 for sizes <= 1MB.
        return n + 1;
    }
    const size_t alignment = 64 * 1024;
    return ((size + alignment - 1) / alignment) * alignment;
}

// Optimization 2: Builtin CLZ
size_t roundToPoolSize_CLZ(size_t size) {
    if (size <= 1024 * 1024) {
        if (size <= 1) return 1;
        // Use uint32_t for sizes <= 1MB to ensure consistent behavior
        // and avoid 64-bit builtin complexities if size_t varies.
        // 1MB fits in 32 bits easily.
        uint32_t v = static_cast<uint32_t>(size - 1);
        // __builtin_clz works on unsigned int (at least 32 bits)
        return 1U << (32 - __builtin_clz(v));
    }
    const size_t alignment = 64 * 1024;
    return ((size + alignment - 1) / alignment) * alignment;
}

int main() {
    // Setup test data
    const int num_iterations = 10000000;
    std::vector<size_t> test_sizes;
    test_sizes.reserve(num_iterations);

    std::mt19937 rng(42);
    // Focus on sizes <= 1MB where the loop is used
    std::uniform_int_distribution<size_t> dist(1, 1024 * 1024);

    for (int i = 0; i < num_iterations; ++i) {
        test_sizes.push_back(dist(rng));
    }

    // Benchmark Baseline
    auto start = std::chrono::high_resolution_clock::now();
    volatile size_t result_baseline = 0;
    for (size_t s : test_sizes) {
        result_baseline = roundToPoolSize_Baseline(s);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed_baseline = end - start;
    std::cout << "Baseline (Loop): " << elapsed_baseline.count() << " ms" << std::endl;

    // Benchmark Bit Trick
    start = std::chrono::high_resolution_clock::now();
    volatile size_t result_bittrick = 0;
    for (size_t s : test_sizes) {
        result_bittrick = roundToPoolSize_BitTrick(s);
    }
    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed_bittrick = end - start;
    std::cout << "Bit Trick:       " << elapsed_bittrick.count() << " ms" << std::endl;

    // Benchmark CLZ
    start = std::chrono::high_resolution_clock::now();
    volatile size_t result_clz = 0;
    for (size_t s : test_sizes) {
        result_clz = roundToPoolSize_CLZ(s);
    }
    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed_clz = end - start;
    std::cout << "Builtin CLZ:     " << elapsed_clz.count() << " ms" << std::endl;

    // Verification
    for (size_t s : test_sizes) {
        size_t b = roundToPoolSize_Baseline(s);
        size_t t = roundToPoolSize_BitTrick(s);
        size_t c = roundToPoolSize_CLZ(s);
        if (b != t || b != c) {
            std::cerr << "Mismatch for size " << s << ": Baseline=" << b << ", BitTrick=" << t << ", CLZ=" << c << std::endl;
            return 1;
        }
    }
    std::cout << "Verification Passed!" << std::endl;

    return 0;
}
