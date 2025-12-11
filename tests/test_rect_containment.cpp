/*
 * test_rect_containment.cpp - Unit tests for Rect containment methods
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Containment Tests
 * @TEST_DESCRIPTION: Tests point and rectangle containment methods for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, containment, points, boundaries
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/core/rect.h"
using PsyMP3::Core::Rect;
#include <iostream>

using namespace TestFramework;
using namespace RectTestUtilities;

void test_point_containment() {
    // Test basic point containment
    Rect rect(10, 10, 20, 20);  // Rectangle from (10,10) to (30,30)
    
    // Points inside the rectangle
    ASSERT_TRUE(rect.contains(15, 15), "Center point should be contained");
    ASSERT_TRUE(rect.contains(10, 10), "Top-left corner should be contained (inclusive)");
    ASSERT_TRUE(rect.contains(29, 29), "Near bottom-right should be contained");
    
    // Points outside the rectangle
    ASSERT_FALSE(rect.contains(5, 15), "Point left of rectangle should not be contained");
    ASSERT_FALSE(rect.contains(35, 15), "Point right of rectangle should not be contained");
    ASSERT_FALSE(rect.contains(15, 5), "Point above rectangle should not be contained");
    ASSERT_FALSE(rect.contains(15, 35), "Point below rectangle should not be contained");
    ASSERT_FALSE(rect.contains(30, 20), "Point on right edge should not be contained (exclusive)");
    ASSERT_FALSE(rect.contains(20, 30), "Point on bottom edge should not be contained (exclusive)");
    
    // Test with rectangle at origin
    Rect origin_rect(0, 0, 10, 10);
    ASSERT_TRUE(origin_rect.contains(0, 0), "Origin point should be contained");
    ASSERT_TRUE(origin_rect.contains(5, 5), "Center of origin rectangle should be contained");
    ASSERT_TRUE(origin_rect.contains(9, 9), "Near boundary of origin rectangle should be contained");
    ASSERT_FALSE(origin_rect.contains(10, 5), "Right edge of origin rectangle should not be contained");
    ASSERT_FALSE(origin_rect.contains(5, 10), "Bottom edge of origin rectangle should not be contained");
    ASSERT_FALSE(origin_rect.contains(-1, 5), "Negative coordinates should not be contained");
    
    // Test with negative coordinates
    Rect neg_rect(-10, -10, 20, 20);  // Rectangle from (-10,-10) to (10,10)
    ASSERT_TRUE(neg_rect.contains(-5, -5), "Point inside negative coordinate rectangle should be contained");
    ASSERT_TRUE(neg_rect.contains(-10, -10), "Top-left corner of negative rectangle should be contained");
    ASSERT_TRUE(neg_rect.contains(9, 9), "Near bottom-right of negative rectangle should be contained");
    ASSERT_FALSE(neg_rect.contains(10, 5), "Right edge of negative rectangle should not be contained");
    ASSERT_FALSE(neg_rect.contains(-11, 0), "Point left of negative rectangle should not be contained");
}

void test_point_containment_empty_rectangles() {
    // Empty rectangles should not contain any points
    Rect empty1 = TestRects::zeroWidth();
    ASSERT_FALSE(empty1.contains(0, 5), "Zero width rectangle should not contain any points");
    ASSERT_FALSE(empty1.contains(5, 5), "Zero width rectangle should not contain any points");
    
    Rect empty2 = TestRects::zeroHeight();
    ASSERT_FALSE(empty2.contains(5, 0), "Zero height rectangle should not contain any points");
    ASSERT_FALSE(empty2.contains(5, 5), "Zero height rectangle should not contain any points");
    
    Rect empty3 = TestRects::empty();
    ASSERT_FALSE(empty3.contains(0, 0), "Empty rectangle should not contain origin");
    ASSERT_FALSE(empty3.contains(1, 1), "Empty rectangle should not contain any points");
}

void test_rectangle_containment() {
    // Test basic rectangle containment
    Rect outer = TestRects::container();
    Rect inner(10, 10, 20, 20);
    
    testContainmentPatterns(outer, inner, true, "Basic containment - outer contains inner");
    testContainmentPatterns(inner, outer, false, "Basic containment - inner should not contain outer");
    
    // Test identical rectangles
    Rect rect1(10, 10, 20, 20);
    Rect rect2(10, 10, 20, 20);
    testContainmentPatterns(rect1, rect2, true, "Identical rectangles should contain each other");
    testContainmentPatterns(rect2, rect1, true, "Identical rectangles should contain each other (symmetric)");
    
    // Test partially overlapping rectangles
    Rect rect3(0, 0, 20, 20);
    Rect rect4(10, 10, 20, 20);
    testContainmentPatterns(rect3, rect4, false, "Partial overlap should not be containment");
    testContainmentPatterns(rect4, rect3, false, "Partial overlap should not be containment (symmetric)");
    
    // Test non-overlapping rectangles
    Rect rect5(0, 0, 10, 10);
    Rect rect6(20, 20, 10, 10);
    testContainmentPatterns(rect5, rect6, false, "Non-overlapping rectangles should not contain each other");
    testContainmentPatterns(rect6, rect5, false, "Non-overlapping rectangles should not contain each other (symmetric)");
    
    // Test edge cases - rectangles touching edges
    Rect container(0, 0, 100, 100);
    Rect edge_rect(0, 0, 100, 100);  // Same size, should contain
    testContainmentPatterns(container, edge_rect, true, "Same size rectangles should contain each other");
    
    Rect too_wide(0, 0, 101, 50);    // Extends beyond right edge
    testContainmentPatterns(container, too_wide, false, "Rectangle extending beyond right edge should not be contained");
    
    Rect too_tall(0, 0, 50, 101);    // Extends beyond bottom edge
    testContainmentPatterns(container, too_tall, false, "Rectangle extending beyond bottom edge should not be contained");
}

void test_rectangle_containment_empty_rectangles() {
    Rect normal = TestRects::standard();
    Rect empty1 = TestRects::zeroWidth();
    Rect empty2 = TestRects::zeroHeight();
    Rect empty3 = TestRects::empty();
    
    // Empty rectangles cannot contain anything
    testContainmentPatterns(empty1, normal, false, "Zero width rectangle cannot contain normal rectangle");
    testContainmentPatterns(empty2, normal, false, "Zero height rectangle cannot contain normal rectangle");
    testContainmentPatterns(empty3, normal, false, "Empty rectangle cannot contain normal rectangle");
    testContainmentPatterns(empty1, empty2, false, "Empty rectangles cannot contain each other");
    
    // Nothing can contain empty rectangles (based on typical implementation)
    testContainmentPatterns(normal, empty1, false, "Normal rectangle should not contain zero width rectangle");
    testContainmentPatterns(normal, empty2, false, "Normal rectangle should not contain zero height rectangle");
    testContainmentPatterns(normal, empty3, false, "Normal rectangle should not contain empty rectangle");
}

void test_single_pixel_rectangles() {
    Rect pixel = TestRects::singlePixel();  // Single pixel at (5,5)
    
    // Point containment
    ASSERT_TRUE(pixel.contains(5, 5), "Single pixel should contain its own point");
    ASSERT_FALSE(pixel.contains(6, 5), "Single pixel should not contain adjacent points");
    ASSERT_FALSE(pixel.contains(5, 6), "Single pixel should not contain adjacent points");
    ASSERT_FALSE(pixel.contains(4, 5), "Single pixel should not contain adjacent points");
    ASSERT_FALSE(pixel.contains(5, 4), "Single pixel should not contain adjacent points");
    
    // Rectangle containment
    Rect container(0, 0, 10, 10);
    testContainmentPatterns(container, pixel, true, "Container should contain single pixel");
    testContainmentPatterns(pixel, container, false, "Single pixel cannot contain larger rectangle");
    
    Rect another_pixel(5, 5, 1, 1);
    testContainmentPatterns(pixel, another_pixel, true, "Identical single pixels should contain each other");
}

int main() {
    // Create test suite
    TestSuite suite("Rectangle Containment Tests");
    
    // Add test functions
    suite.addTest("Point Containment", test_point_containment);
    suite.addTest("Point Containment with Empty Rectangles", test_point_containment_empty_rectangles);
    suite.addTest("Rectangle Containment", test_rectangle_containment);
    suite.addTest("Rectangle Containment with Empty Rectangles", test_rectangle_containment_empty_rectangles);
    suite.addTest("Single Pixel Rectangle Containment", test_single_pixel_rectangles);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}