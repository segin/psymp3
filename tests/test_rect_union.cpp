/*
 * test_rect_union.cpp - Unit tests for Rect union method
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Union Tests
 * @TEST_DESCRIPTION: Tests union calculation methods for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, union, bounding, geometry
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/rect.h"
#include <iostream>

using namespace TestFramework;
using namespace RectTestUtilities;

void test_union_basic() {
    // Test union of overlapping rectangles
    Rect rect1(0, 0, 20, 20);
    Rect rect2(10, 10, 20, 20);
    Rect expected_union(0, 0, 30, 30);
    testUnionPatterns(rect1, rect2, expected_union, "Overlapping rectangles union");
    
    // Test union of non-overlapping rectangles
    Rect rect3(0, 0, 10, 10);
    Rect rect4(20, 20, 10, 10);
    Rect expected_union2(0, 0, 30, 30);
    testUnionPatterns(rect3, rect4, expected_union2, "Non-overlapping rectangles union");
    
    // Test union of identical rectangles
    Rect rect5(5, 5, 15, 15);
    Rect rect6(5, 5, 15, 15);
    testUnionPatterns(rect5, rect6, rect5, "Identical rectangles union");
}

void test_union_containment() {
    // Test union where one rectangle contains the other
    Rect outer = TestRects::container();
    Rect inner(10, 10, 20, 20);
    
    testUnionPatterns(outer, inner, outer, "Container union with inner rectangle");
}

void test_union_with_empty_rectangles() {
    Rect normal = TestRects::standard();
    Rect empty1 = TestRects::zeroWidth();
    Rect empty2 = TestRects::zeroHeight();
    Rect empty3 = TestRects::empty();
    
    // Union with empty rectangle should return the non-empty rectangle
    testUnionPatterns(normal, empty1, normal, "Normal union with zero width");
    testUnionPatterns(normal, empty2, normal, "Normal union with zero height");
    testUnionPatterns(normal, empty3, normal, "Normal union with empty");
    
    // Union of two empty rectangles should return empty
    testUnionPatterns(empty1, empty2, TestRects::empty(), "Zero width union with zero height");
    testUnionPatterns(empty3, empty3, TestRects::empty(), "Empty union with empty");
}

void test_union_various_positions() {
    Rect base(10, 10, 10, 10);  // Rectangle from (10,10) to (20,20)
    
    // Union with rectangle to the left
    Rect left(0, 10, 10, 10);   // (0,10) to (10,20)
    Rect expected_left(0, 10, 20, 10);
    testUnionPatterns(base, left, expected_left, "Union with left rectangle");
    
    // Union with rectangle to the right
    Rect right(20, 10, 10, 10); // (20,10) to (30,20)
    Rect expected_right(10, 10, 20, 10);
    testUnionPatterns(base, right, expected_right, "Union with right rectangle");
    
    // Union with rectangle above
    Rect above(10, 0, 10, 10);  // (10,0) to (20,10)
    Rect expected_above(10, 0, 10, 20);
    testUnionPatterns(base, above, expected_above, "Union with above rectangle");
    
    // Union with rectangle below
    Rect below(10, 20, 10, 10); // (10,20) to (20,30)
    Rect expected_below(10, 10, 10, 20);
    testUnionPatterns(base, below, expected_below, "Union with below rectangle");
    
    // Union with diagonal rectangle
    Rect diagonal(0, 0, 5, 5);  // (0,0) to (5,5)
    Rect expected_diagonal(0, 0, 20, 20);
    testUnionPatterns(base, diagonal, expected_diagonal, "Union with diagonal rectangle");
}

void test_union_negative_coordinates() {
    // Test union with negative coordinates
    Rect pos(10, 10, 10, 10);   // (10,10) to (20,20)
    Rect neg = TestRects::withNegativeCoords(); // (-10,-10) to (10,10)
    
    Rect expected_result(-10, -10, 30, 30);
    testUnionPatterns(pos, neg, expected_result, "Union with negative coordinates");
    
    // Test union spanning across zero
    Rect span1(-5, -5, 10, 10); // (-5,-5) to (5,5)
    Rect span2(5, 5, 10, 10);   // (5,5) to (15,15)
    
    Rect expected_span(-5, -5, 20, 20);
    testUnionPatterns(span1, span2, expected_span, "Union spanning across zero");
}

void test_union_overflow_handling() {
    // Test potential overflow scenarios
    Rect rect1(0, 0, 32767, 32767);        // Large rectangle
    Rect rect2(32767, 32767, 32767, 32767); // Another large rectangle
    
    Rect expected_result(0, 0, 65534, 65534);
    testUnionPatterns(rect1, rect2, expected_result, "Union with potential overflow");
    
    // Test with maximum values that would cause overflow
    Rect max1(-10000, -10000, 65535, 65535);
    Rect max2(10000, 10000, 65535, 65535);
    
    Rect expected_max(-10000, -10000, 65535, 65535);  // Should be clamped
    testUnionPatterns(max1, max2, expected_max, "Union with overflow clamping");
}

void test_union_single_pixel() {
    // Test union of single pixel rectangles
    Rect pixel1(10, 10, 1, 1);
    Rect pixel2(12, 12, 1, 1);
    
    Rect expected_result(10, 10, 3, 3);
    testUnionPatterns(pixel1, pixel2, expected_result, "Union of single pixels");
    
    // Test union of single pixel with larger rectangle
    Rect large(0, 0, 20, 20);
    Rect pixel(25, 25, 1, 1);
    
    Rect expected_large(0, 0, 26, 26);
    testUnionPatterns(large, pixel, expected_large, "Union of large rectangle with single pixel");
}

int main() {
    // Create test suite
    TestSuite suite("Rectangle Union Tests");
    
    // Add test functions
    suite.addTest("Basic Union", test_union_basic);
    suite.addTest("Union with Containment", test_union_containment);
    suite.addTest("Union with Empty Rectangles", test_union_with_empty_rectangles);
    suite.addTest("Union with Various Positions", test_union_various_positions);
    suite.addTest("Union with Negative Coordinates", test_union_negative_coordinates);
    suite.addTest("Union Overflow Handling", test_union_overflow_handling);
    suite.addTest("Union with Single Pixel", test_union_single_pixel);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}