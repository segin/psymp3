/*
 * test_rect_performance.cpp - Performance validation for enhanced Rect class
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/core/rect.h"
using PsyMP3::Core::Rect;
#include <iostream>
#include <chrono>
#include <vector>

using namespace TestFramework;

/**
 * Test performance of basic accessor methods (hot path operations)
 */
void test_accessor_performance() {
    const int iterations = 1000000;
    std::vector<Rect> rects;
    rects.reserve(iterations);
    
    // Create test rectangles
    for (int i = 0; i < iterations; ++i) {
        rects.emplace_back(i % 1000, (i * 2) % 1000, (i * 3) % 500 + 1, (i * 4) % 500 + 1);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    volatile int sum = 0;  // Prevent optimization
    for (const auto& rect : rects) {
        sum += rect.x() + rect.y() + rect.width() + rect.height();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Accessor performance: " << iterations << " operations in " 
              << duration.count() << " microseconds" << std::endl;
    
    // Should complete in reasonable time (less than 100ms for 1M operations)
    ASSERT_TRUE(duration.count() < 100000, "Accessor methods should be fast");
}

/**
 * Test performance of geometric operations
 */
void test_geometric_performance() {
    const int iterations = 100000;
    std::vector<Rect> rects1, rects2;
    rects1.reserve(iterations);
    rects2.reserve(iterations);
    
    // Create test rectangles
    for (int i = 0; i < iterations; ++i) {
        rects1.emplace_back(i % 500, (i * 2) % 500, 100, 100);
        rects2.emplace_back((i + 50) % 500, ((i + 50) * 2) % 500, 100, 100);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    volatile int count = 0;  // Prevent optimization
    for (int i = 0; i < iterations; ++i) {
        if (rects1[i].intersects(rects2[i])) {
            count++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Geometric performance: " << iterations << " intersections in " 
              << duration.count() << " microseconds" << std::endl;
    
    // Should complete in reasonable time (less than 50ms for 100K operations)
    ASSERT_TRUE(duration.count() < 50000, "Geometric operations should be fast");
}

/**
 * Test performance of point containment (used in mouse hit testing)
 */
void test_containment_performance() {
    const int iterations = 100000;
    std::vector<Rect> rects;
    rects.reserve(iterations);
    
    // Create test rectangles
    for (int i = 0; i < iterations; ++i) {
        rects.emplace_back(i % 500, (i * 2) % 500, 100, 100);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    volatile int count = 0;  // Prevent optimization
    for (int i = 0; i < iterations; ++i) {
        if (rects[i].contains(i % 600, (i * 2) % 600)) {
            count++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Containment performance: " << iterations << " point tests in " 
              << duration.count() << " microseconds" << std::endl;
    
    // Should complete in reasonable time (less than 30ms for 100K operations)
    ASSERT_TRUE(duration.count() < 30000, "Point containment should be fast");
}

/**
 * Test memory usage and object size
 */
void test_memory_usage() {
    // Verify that Rect size hasn't changed
    size_t rect_size = sizeof(Rect);
    std::cout << "Rect object size: " << rect_size << " bytes" << std::endl;
    
    // Should be exactly 8 bytes (4 × 16-bit values)
    ASSERT_TRUE(rect_size == 8, "Rect size should remain 8 bytes for compatibility");
    
    // Test that large numbers of Rect objects can be created efficiently
    const int count = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<Rect> rects;
    rects.reserve(count);
    for (int i = 0; i < count; ++i) {
        rects.emplace_back(i, i * 2, i * 3 + 1, i * 4 + 1);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Memory allocation: " << count << " objects in " 
              << duration.count() << " microseconds" << std::endl;
    
    // Should complete quickly (less than 10ms for 10K objects)
    ASSERT_TRUE(duration.count() < 10000, "Object creation should be fast");
}

int main() {
    TestSuite suite("Rectangle Performance Tests");
    
    suite.addTest("Accessor Performance", test_accessor_performance);
    suite.addTest("Geometric Performance", test_geometric_performance);
    suite.addTest("Containment Performance", test_containment_performance);
    suite.addTest("Memory Usage", test_memory_usage);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}