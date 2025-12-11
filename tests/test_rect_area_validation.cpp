/*
 * test_rect_area_validation.cpp - Unit tests for Rect area and validation methods
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Area Validation Tests
 * @TEST_DESCRIPTION: Tests area calculation, isEmpty, and isValid methods for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, area, validation, basic
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/core/rect.h"
using PsyMP3::Core::Rect;
#include <iostream>
#include <limits>

using namespace TestFramework;
using namespace RectTestUtilities;

void test_area_calculation() {
    // Test basic area calculation
    Rect rect1(10, 20);
    assertRectArea(rect1, 200, "Basic area calculation");
    
    // Test area with position (position shouldn't affect area)
    Rect rect2(5, 5, 10, 20);
    assertRectArea(rect2, 200, "Area calculation with position");
    
    // Test zero area cases
    Rect rect3(0, 10);
    assertRectArea(rect3, 0, "Zero width area");
    
    Rect rect4(10, 0);
    assertRectArea(rect4, 0, "Zero height area");
    
    Rect rect5(0, 0);
    assertRectArea(rect5, 0, "Zero width and height area");
    
    // Test single pixel rectangle
    Rect rect6(1, 1);
    assertRectArea(rect6, 1, "Single pixel area");
    
    // Test large area calculation (potential overflow handling)
    Rect rect7(65535, 65535);  // Max uint16_t values
    uint32_t expected_area = static_cast<uint32_t>(65535) * static_cast<uint32_t>(65535);
    assertRectArea(rect7, expected_area, "Large area calculation");
}

void test_isEmpty() {
    // Test non-empty rectangle
    Rect rect1(10, 20);
    assertRectNotEmpty(rect1, "Non-empty rectangle");
    
    // Test rectangle with zero width
    Rect rect2(0, 20);
    assertRectEmpty(rect2, "Zero width rectangle");
    
    // Test rectangle with zero height
    Rect rect3(10, 0);
    assertRectEmpty(rect3, "Zero height rectangle");
    
    // Test rectangle with both zero
    Rect rect4(0, 0);
    assertRectEmpty(rect4, "Zero width and height rectangle");
    
    // Test single pixel rectangle (not empty)
    Rect rect5(1, 1);
    assertRectNotEmpty(rect5, "Single pixel rectangle");
    
    // Test with position (position shouldn't affect emptiness)
    Rect rect6(-10, -10, 0, 20);
    assertRectEmpty(rect6, "Zero width with negative position");
    
    Rect rect7(-10, -10, 20, 0);
    assertRectEmpty(rect7, "Zero height with negative position");
    
    Rect rect8(-10, -10, 20, 20);
    assertRectNotEmpty(rect8, "Non-empty with negative position");
}

void test_isValid() {
    // Test valid rectangle
    Rect rect1(10, 20);
    assertRectValid(rect1, "Valid rectangle");
    
    // Test invalid rectangle with zero width
    Rect rect2(0, 20);
    assertRectInvalid(rect2, "Invalid zero width rectangle");
    
    // Test invalid rectangle with zero height
    Rect rect3(10, 0);
    assertRectInvalid(rect3, "Invalid zero height rectangle");
    
    // Test invalid rectangle with both zero
    Rect rect4(0, 0);
    assertRectInvalid(rect4, "Invalid zero width and height rectangle");
    
    // Test single pixel rectangle (valid)
    Rect rect5(1, 1);
    assertRectValid(rect5, "Valid single pixel rectangle");
    
    // Test with position (position shouldn't affect validity)
    Rect rect6(-10, -10, 0, 20);
    assertRectInvalid(rect6, "Invalid zero width with negative position");
    
    Rect rect7(-10, -10, 20, 0);
    assertRectInvalid(rect7, "Invalid zero height with negative position");
    
    Rect rect8(-10, -10, 20, 20);
    assertRectValid(rect8, "Valid rectangle with negative position");
    
    // Test maximum valid rectangle
    Rect rect9(65535, 65535);
    assertRectValid(rect9, "Maximum valid rectangle");
}

void test_consistency_between_isEmpty_and_isValid() {
    // For rectangles with positive dimensions, isEmpty and isValid should be opposites
    Rect rect1(10, 20);
    assertRectNotEmpty(rect1, "Positive dimensions should not be empty");
    assertRectValid(rect1, "Positive dimensions should be valid");
    
    // For rectangles with zero width or height, both should indicate problems
    Rect rect2(0, 20);
    assertRectEmpty(rect2, "Zero width should be empty");
    assertRectInvalid(rect2, "Zero width should be invalid");
    
    Rect rect3(10, 0);
    assertRectEmpty(rect3, "Zero height should be empty");
    assertRectInvalid(rect3, "Zero height should be invalid");
    
    Rect rect4(0, 0);
    assertRectEmpty(rect4, "Zero dimensions should be empty");
    assertRectInvalid(rect4, "Zero dimensions should be invalid");
}

int main() {
    // Create test suite
    TestSuite suite("Rectangle Area and Validation Tests");
    
    // Add test functions
    suite.addTest("Area Calculation", test_area_calculation);
    suite.addTest("isEmpty Method", test_isEmpty);
    suite.addTest("isValid Method", test_isValid);
    suite.addTest("isEmpty/isValid Consistency", test_consistency_between_isEmpty_and_isValid);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}