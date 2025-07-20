/*
 * test_rect_centering.cpp - Test centering operations for Rect class
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Centering Tests
 * @TEST_DESCRIPTION: Tests centering operations for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, centering, positioning
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/rect.h"
#include <iostream>

using namespace TestFramework;
using namespace RectTestUtilities;

void test_basic_centering() {
    // Test centering a 10x10 rectangle in a 100x100 container at origin
    Rect container = TestRects::container();  // Container at (0,0) with size 100x100
    Rect rect(0, 0, 10, 10);                  // Rectangle at (0,0) with size 10x10
    
    rect.centerIn(container);
    
    // Container center is at (50, 50)
    // Rectangle should be positioned at (45, 45) to center it
    assertRectEquals(rect, 45, 45, 10, 10, "Basic centering in container");
}

void test_centering_with_offset_container() {
    // Test centering in a container that's not at origin
    Rect container = TestRects::offsetContainer();  // Container at (20,30) with size 60x40
    Rect rect(0, 0, 20, 10);                       // Rectangle at (0,0) with size 20x10
    
    rect.centerIn(container);
    
    // Container center is at (20 + 60/2, 30 + 40/2) = (50, 50)
    // Rectangle should be positioned at (50 - 20/2, 50 - 10/2) = (40, 45)
    assertRectEquals(rect, 40, 45, 20, 10, "Centering in offset container");
}

void test_centering_larger_rectangle() {
    // Test centering a rectangle that's larger than the container
    Rect container(10, 10, 50, 50);  // Container at (10,10) with size 50x50
    Rect rect(0, 0, 100, 80);        // Rectangle at (0,0) with size 100x80
    
    rect.centerIn(container);
    
    // Container center is at (10 + 50/2, 10 + 50/2) = (35, 35)
    // Rectangle should be positioned at (35 - 100/2, 35 - 80/2) = (-15, -5)
    assertRectEquals(rect, -15, -5, 100, 80, "Centering rectangle larger than container");
}

void test_centered_in_method() {
    // Test the const version that returns a new rectangle
    Rect container = TestRects::container();
    Rect original(5, 5, 20, 20);
    
    Rect centered = original.centeredIn(container);
    
    // Original should be unchanged
    assertRectEquals(original, 5, 5, 20, 20, "Original should be unchanged after centeredIn()");
    
    // Centered should be positioned at (40, 40) to center the 20x20 rect in 100x100 container
    assertRectEquals(centered, 40, 40, 20, 20, "Centered rectangle position");
}

void test_edge_cases() {
    // Test with zero-size rectangle
    Rect container = TestRects::container();
    Rect zero_rect(10, 10, 0, 0);
    
    zero_rect.centerIn(container);
    
    // Zero-size rectangle should be centered at container center
    assertRectEquals(zero_rect, 50, 50, 0, 0, "Zero-size rectangle centering");
    
    // Test with zero-size container
    Rect zero_container(25, 25, 0, 0);
    Rect rect(0, 0, 10, 10);
    
    rect.centerIn(zero_container);
    
    // Rectangle should be centered at container position
    assertRectEquals(rect, 20, 20, 10, 10, "Centering in zero-size container");
}

int main() {
    // Create test suite
    TestSuite suite("Rectangle Centering Tests");
    
    // Add test functions
    suite.addTest("Basic Centering", test_basic_centering);
    suite.addTest("Centering with Offset Container", test_centering_with_offset_container);
    suite.addTest("Centering Larger Rectangle", test_centering_larger_rectangle);
    suite.addTest("centeredIn Method", test_centered_in_method);
    suite.addTest("Edge Cases", test_edge_cases);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}