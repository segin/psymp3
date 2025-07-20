/*
 * test_rect_area_validation.cpp - Unit tests for Rect area and validation methods
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <cassert>
#include <iostream>
#include <limits>
#include "../include/rect.h"

void test_area_calculation() {
    std::cout << "Testing area calculation..." << std::endl;
    
    // Test basic area calculation
    Rect rect1(10, 20);
    assert(rect1.area() == 200);
    
    // Test area with position (position shouldn't affect area)
    Rect rect2(5, 5, 10, 20);
    assert(rect2.area() == 200);
    
    // Test zero area cases
    Rect rect3(0, 10);
    assert(rect3.area() == 0);
    
    Rect rect4(10, 0);
    assert(rect4.area() == 0);
    
    Rect rect5(0, 0);
    assert(rect5.area() == 0);
    
    // Test single pixel rectangle
    Rect rect6(1, 1);
    assert(rect6.area() == 1);
    
    // Test large area calculation (potential overflow handling)
    Rect rect7(65535, 65535);  // Max uint16_t values
    uint32_t expected_area = static_cast<uint32_t>(65535) * static_cast<uint32_t>(65535);
    assert(rect7.area() == expected_area);
    
    std::cout << "Area calculation tests passed!" << std::endl;
}

void test_isEmpty() {
    std::cout << "Testing isEmpty method..." << std::endl;
    
    // Test non-empty rectangle
    Rect rect1(10, 20);
    assert(!rect1.isEmpty());
    
    // Test rectangle with zero width
    Rect rect2(0, 20);
    assert(rect2.isEmpty());
    
    // Test rectangle with zero height
    Rect rect3(10, 0);
    assert(rect3.isEmpty());
    
    // Test rectangle with both zero
    Rect rect4(0, 0);
    assert(rect4.isEmpty());
    
    // Test single pixel rectangle (not empty)
    Rect rect5(1, 1);
    assert(!rect5.isEmpty());
    
    // Test with position (position shouldn't affect emptiness)
    Rect rect6(-10, -10, 0, 20);
    assert(rect6.isEmpty());
    
    Rect rect7(-10, -10, 20, 0);
    assert(rect7.isEmpty());
    
    Rect rect8(-10, -10, 20, 20);
    assert(!rect8.isEmpty());
    
    std::cout << "isEmpty tests passed!" << std::endl;
}

void test_isValid() {
    std::cout << "Testing isValid method..." << std::endl;
    
    // Test valid rectangle
    Rect rect1(10, 20);
    assert(rect1.isValid());
    
    // Test invalid rectangle with zero width
    Rect rect2(0, 20);
    assert(!rect2.isValid());
    
    // Test invalid rectangle with zero height
    Rect rect3(10, 0);
    assert(!rect3.isValid());
    
    // Test invalid rectangle with both zero
    Rect rect4(0, 0);
    assert(!rect4.isValid());
    
    // Test single pixel rectangle (valid)
    Rect rect5(1, 1);
    assert(rect5.isValid());
    
    // Test with position (position shouldn't affect validity)
    Rect rect6(-10, -10, 0, 20);
    assert(!rect6.isValid());
    
    Rect rect7(-10, -10, 20, 0);
    assert(!rect7.isValid());
    
    Rect rect8(-10, -10, 20, 20);
    assert(rect8.isValid());
    
    // Test maximum valid rectangle
    Rect rect9(65535, 65535);
    assert(rect9.isValid());
    
    std::cout << "isValid tests passed!" << std::endl;
}

void test_consistency_between_isEmpty_and_isValid() {
    std::cout << "Testing consistency between isEmpty and isValid..." << std::endl;
    
    // For rectangles with positive dimensions, isEmpty and isValid should be opposites
    Rect rect1(10, 20);
    assert(!rect1.isEmpty() && rect1.isValid());
    
    // For rectangles with zero width or height, both should indicate problems
    Rect rect2(0, 20);
    assert(rect2.isEmpty() && !rect2.isValid());
    
    Rect rect3(10, 0);
    assert(rect3.isEmpty() && !rect3.isValid());
    
    Rect rect4(0, 0);
    assert(rect4.isEmpty() && !rect4.isValid());
    
    std::cout << "Consistency tests passed!" << std::endl;
}

int main() {
    std::cout << "Running Rect area and validation method tests..." << std::endl;
    
    try {
        test_area_calculation();
        test_isEmpty();
        test_isValid();
        test_consistency_between_isEmpty_and_isValid();
        
        std::cout << "All tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}