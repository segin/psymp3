/*
 * test_rect_modern_cpp.cpp - Test modern C++ features of Rect class
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Modern C++ Features Tests
 * @TEST_DESCRIPTION: Tests modern C++ features like operators and toString for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, operators, toString, modern-cpp
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/core/rect.h"
using PsyMP3::Core::Rect;
#include <iostream>
#include <string>

using namespace TestFramework;
using namespace RectTestUtilities;

void test_comparison_operators() {
    // Test comparison operators
    Rect rect1 = TestRects::standard();
    Rect rect2 = TestRects::standard();
    Rect rect3(15, 25, 100, 200);
    
    // Test equality operator
    ASSERT_TRUE(rect1 == rect2, "Identical rectangles should be equal");
    ASSERT_FALSE(rect1 == rect3, "Different rectangles should not be equal");
    
    // Test inequality operator
    ASSERT_FALSE(rect1 != rect2, "Identical rectangles should not be unequal");
    ASSERT_TRUE(rect1 != rect3, "Different rectangles should be unequal");
}

void test_toString_method() {
    // Test toString method
    Rect rect1 = TestRects::standard();
    std::string str1 = rect1.toString();
    std::string expected1 = "Rect(10, 20, 100, 50)";
    ASSERT_EQUALS(str1, expected1, "toString() for valid rectangle");
    
    // Test toString with empty rectangle
    Rect emptyRect = TestRects::zeroWidth();
    std::string str2 = emptyRect.toString();
    ASSERT_TRUE(str2.find("[EMPTY]") != std::string::npos, "toString() should indicate empty rectangle");
    
    // Test toString with invalid rectangle (zero height)
    Rect invalidRect = TestRects::zeroHeight();
    std::string str3 = invalidRect.toString();
    ASSERT_TRUE(str3.find("[EMPTY]") != std::string::npos, "toString() should indicate invalid rectangle");
}

int main() {
    // Create test suite
    TestSuite suite("Rectangle Modern C++ Features Tests");
    
    // Add test functions
    suite.addTest("Comparison Operators", test_comparison_operators);
    suite.addTest("toString Method", test_toString_method);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}