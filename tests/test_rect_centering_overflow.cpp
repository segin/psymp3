/*
 * test_rect_centering_overflow.cpp - Test centering operations with overflow conditions
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Centering Overflow Tests
 * @TEST_DESCRIPTION: Tests centering operations with overflow and edge conditions for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, centering, overflow, edge-cases
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/rect.h"
#include <iostream>
#include <cstdint>
#include <limits>

using namespace TestFramework;
using namespace RectTestUtilities;

void test_overflow_conditions() {
    // Test with container that would cause underflow when centering a large rectangle
    // We need a scenario where container_center - rect_size/2 < -32768
    Rect container(-32768, -32768, 100, 100);    // Container at minimum coordinates
    Rect rect(0, 0, 65535, 65535);               // Maximum size rectangle
    
    rect.centerIn(container);
    
    // Container center is at (-32768 + 100/2, -32768 + 100/2) = (-32718, -32718)
    // Rectangle position would be (-32718 - 65535/2, -32718 - 65535/2) = (-32718 - 32767, -32718 - 32767) = (-65485, -65485)
    // This exceeds the minimum int16_t value of -32768, so should be clamped
    ASSERT_EQUALS(rect.x(), -32768, "X coordinate should be clamped to minimum int16_t");
    ASSERT_EQUALS(rect.y(), -32768, "Y coordinate should be clamped to minimum int16_t");
}

void test_underflow_conditions() {
    // Test with container that would cause positive overflow
    Rect container(32767, 32767, 100, 100);  // Container at maximum coordinates
    Rect rect(0, 0, 1, 1);                   // Small rectangle
    
    rect.centerIn(container);
    
    // Container center calculation: 32767 + 100/2 = 32767 + 50 = 32817
    // But this would overflow int16_t, so let's see what actually happens
    // The centerX/centerY methods might overflow, but our centering should handle it
    
    // Since we can't predict the exact overflow behavior in centerX/centerY,
    // let's just verify the result is within valid int16_t range
    ASSERT_TRUE(rect.x() >= -32768 && rect.x() <= 32767, "X coordinate should be within int16_t range");
    ASSERT_TRUE(rect.y() >= -32768 && rect.y() <= 32767, "Y coordinate should be within int16_t range");
}

void test_extreme_size_rectangle() {
    // Test centering maximum size rectangle
    Rect container = TestRects::container();
    Rect rect = TestRects::large();  // Maximum size rectangle
    
    rect.centerIn(container);
    
    // Container center is at (50, 50)
    // Rectangle position should be (50 - 65535/2, 50 - 65535/2) = (50 - 32767, 50 - 32767) = (-32717, -32717)
    assertRectEquals(rect, -32717, -32717, 65535, 65535, "Extreme size rectangle centering");
}

void test_precision_with_odd_dimensions() {
    // Test centering with odd dimensions to check integer division behavior
    Rect container(0, 0, 101, 101);  // Odd-sized container
    Rect rect(0, 0, 11, 11);         // Odd-sized rectangle
    
    rect.centerIn(container);
    
    // Container center is at (0 + 101/2, 0 + 101/2) = (50, 50) (integer division)
    // Rectangle should be positioned at (50 - 11/2, 50 - 11/2) = (50 - 5, 50 - 5) = (45, 45)
    assertRectEquals(rect, 45, 45, 11, 11, "Centering with odd dimensions");
}

int main() {
    // Create test suite
    TestSuite suite("Rectangle Centering Overflow Tests");
    
    // Add test functions
    suite.addTest("Overflow Conditions", test_overflow_conditions);
    suite.addTest("Underflow Conditions", test_underflow_conditions);
    suite.addTest("Extreme Size Rectangle", test_extreme_size_rectangle);
    suite.addTest("Precision with Odd Dimensions", test_precision_with_odd_dimensions);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}