/*
 * test_rect_intersection.cpp - Unit tests for Rect intersection methods
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Intersection Tests
 * @TEST_DESCRIPTION: Tests intersection detection and calculation methods for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, intersection, overlap, geometry
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/core/rect.h"
using PsyMP3::Core::Rect;
#include <iostream>

using namespace TestFramework;
using namespace RectTestUtilities;

void test_intersects_basic() {
    // Test overlapping rectangles
    Rect rect1(0, 0, 20, 20);
    Rect rect2(10, 10, 20, 20);
    Rect expected_intersection(10, 10, 10, 10);
    testIntersectionPatterns(rect1, rect2, true, expected_intersection, "Basic overlapping rectangles");
    
    // Test non-overlapping rectangles
    Rect rect3(0, 0, 10, 10);
    Rect rect4(20, 20, 10, 10);
    testIntersectionPatterns(rect3, rect4, false, TestRects::empty(), "Non-overlapping rectangles");
    
    // Test adjacent rectangles (touching edges, no overlap)
    Rect rect5(0, 0, 10, 10);
    Rect rect6(10, 0, 10, 10);  // Right adjacent
    testIntersectionPatterns(rect5, rect6, false, TestRects::empty(), "Right adjacent rectangles");
    
    Rect rect7(0, 10, 10, 10);  // Bottom adjacent
    testIntersectionPatterns(rect5, rect7, false, TestRects::empty(), "Bottom adjacent rectangles");
    
    // Test identical rectangles
    Rect rect8(5, 5, 15, 15);
    Rect rect9(5, 5, 15, 15);
    testIntersectionPatterns(rect8, rect9, true, rect8, "Identical rectangles");
}

void test_intersects_edge_cases() {
    // Test with empty rectangles
    Rect normal = TestRects::standard();
    Rect empty1 = TestRects::zeroWidth();
    Rect empty2 = TestRects::zeroHeight();
    Rect empty3 = TestRects::empty();
    
    testIntersectionPatterns(normal, empty1, false, TestRects::empty(), "Normal rectangle with zero width");
    testIntersectionPatterns(normal, empty2, false, TestRects::empty(), "Normal rectangle with zero height");
    testIntersectionPatterns(normal, empty3, false, TestRects::empty(), "Normal rectangle with empty");
    testIntersectionPatterns(empty1, empty2, false, TestRects::empty(), "Zero width with zero height");
    
    // Test single pixel rectangles
    Rect pixel1(10, 10, 1, 1);
    Rect pixel2(10, 10, 1, 1);  // Same position
    Rect pixel3(11, 10, 1, 1);  // Adjacent
    Rect pixel4(9, 9, 3, 3);    // Overlapping
    
    testIntersectionPatterns(pixel1, pixel2, true, pixel1, "Identical single pixels");
    testIntersectionPatterns(pixel1, pixel3, false, TestRects::empty(), "Adjacent single pixels");
    testIntersectionPatterns(pixel1, pixel4, true, pixel1, "Single pixel with overlapping rectangle");
}

void test_intersection_calculation() {
    // Test basic intersection
    Rect rect1(0, 0, 20, 20);
    Rect rect2(10, 10, 20, 20);
    Rect expected_result(10, 10, 10, 10);
    
    Rect result = rect1.intersection(rect2);
    assertRectEquals(result, 10, 10, 10, 10, "Basic intersection calculation");
    
    // Test symmetric intersection
    Rect result2 = rect2.intersection(rect1);
    assertRectsIdentical(result, result2, "Intersection should be symmetric");
    
    // Test non-overlapping rectangles (should return empty)
    Rect rect3(0, 0, 10, 10);
    Rect rect4(20, 20, 10, 10);
    Rect empty_result = rect3.intersection(rect4);
    
    assertRectEmpty(empty_result, "Non-overlapping rectangles should return empty intersection");
    
    // Test identical rectangles
    Rect rect5(5, 5, 15, 15);
    Rect rect6(5, 5, 15, 15);
    Rect identical_result = rect5.intersection(rect6);
    
    assertRectEquals(identical_result, 5, 5, 15, 15, "Identical rectangles intersection");
}

void test_intersection_various_overlaps() {
    // Test partial overlap from different directions
    Rect base(10, 10, 20, 20);  // Rectangle from (10,10) to (30,30)
    
    // Overlap from left
    Rect left_overlap(5, 15, 10, 10);  // (5,15) to (15,25)
    Rect expected_left(10, 15, 5, 10);
    testIntersectionPatterns(base, left_overlap, true, expected_left, "Left overlap");
    
    // Overlap from right
    Rect right_overlap(25, 15, 10, 10);  // (25,15) to (35,25)
    Rect expected_right(25, 15, 5, 10);
    testIntersectionPatterns(base, right_overlap, true, expected_right, "Right overlap");
    
    // Overlap from top
    Rect top_overlap(15, 5, 10, 10);  // (15,5) to (25,15)
    Rect expected_top(15, 10, 10, 5);
    testIntersectionPatterns(base, top_overlap, true, expected_top, "Top overlap");
    
    // Overlap from bottom
    Rect bottom_overlap(15, 25, 10, 10);  // (15,25) to (25,35)
    Rect expected_bottom(15, 25, 10, 5);
    testIntersectionPatterns(base, bottom_overlap, true, expected_bottom, "Bottom overlap");
    
    // Complete containment
    Rect inner(15, 15, 5, 5);
    testIntersectionPatterns(base, inner, true, inner, "Complete containment");
}

void test_intersection_with_empty_rectangles() {
    Rect normal = TestRects::standard();
    Rect empty1 = TestRects::zeroWidth();
    Rect empty2 = TestRects::zeroHeight();
    Rect empty3 = TestRects::empty();
    
    // Intersection with empty rectangles should return empty
    testIntersectionPatterns(normal, empty1, false, TestRects::empty(), "Normal with zero width");
    testIntersectionPatterns(normal, empty2, false, TestRects::empty(), "Normal with zero height");
    testIntersectionPatterns(normal, empty3, false, TestRects::empty(), "Normal with empty");
    
    // Empty with empty
    testIntersectionPatterns(empty1, empty2, false, TestRects::empty(), "Zero width with zero height");
}

void test_intersection_consistency() {
    // If intersects() returns false, intersection() should return empty rectangle
    Rect rect1(0, 0, 10, 10);
    Rect rect2(20, 20, 10, 10);
    
    ASSERT_FALSE(rect1.intersects(rect2), "Non-overlapping rectangles should not intersect");
    Rect result = rect1.intersection(rect2);
    assertRectEmpty(result, "Non-intersecting rectangles should return empty intersection");
    
    // If intersects() returns true, intersection() should return non-empty rectangle
    Rect rect3(0, 0, 20, 20);
    Rect rect4(10, 10, 20, 20);
    
    ASSERT_TRUE(rect3.intersects(rect4), "Overlapping rectangles should intersect");
    Rect result2 = rect3.intersection(rect4);
    assertRectNotEmpty(result2, "Intersecting rectangles should return non-empty intersection");
    assertRectValid(result2, "Intersection result should be valid");
}

int main() {
    // Create test suite
    TestSuite suite("Rectangle Intersection Tests");
    
    // Add test functions
    suite.addTest("Basic Intersection Detection", test_intersects_basic);
    suite.addTest("Intersection Edge Cases", test_intersects_edge_cases);
    suite.addTest("Intersection Calculation", test_intersection_calculation);
    suite.addTest("Various Overlap Patterns", test_intersection_various_overlaps);
    suite.addTest("Intersection with Empty Rectangles", test_intersection_with_empty_rectangles);
    suite.addTest("Intersection Method Consistency", test_intersection_consistency);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}