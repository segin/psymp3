/*
 * test_rect_intersection.cpp - Unit tests for Rect intersection methods
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <cassert>
#include <iostream>
#include "../include/rect.h"

void test_intersects_basic() {
    std::cout << "Testing basic rectangle intersection detection..." << std::endl;
    
    // Test overlapping rectangles
    Rect rect1(0, 0, 20, 20);
    Rect rect2(10, 10, 20, 20);
    assert(rect1.intersects(rect2));
    assert(rect2.intersects(rect1)); // Intersection should be symmetric
    
    // Test non-overlapping rectangles
    Rect rect3(0, 0, 10, 10);
    Rect rect4(20, 20, 10, 10);
    assert(!rect3.intersects(rect4));
    assert(!rect4.intersects(rect3));
    
    // Test adjacent rectangles (touching edges, no overlap)
    Rect rect5(0, 0, 10, 10);
    Rect rect6(10, 0, 10, 10);  // Right adjacent
    assert(!rect5.intersects(rect6));
    assert(!rect6.intersects(rect5));
    
    Rect rect7(0, 10, 10, 10);  // Bottom adjacent
    assert(!rect5.intersects(rect7));
    assert(!rect7.intersects(rect5));
    
    // Test identical rectangles
    Rect rect8(5, 5, 15, 15);
    Rect rect9(5, 5, 15, 15);
    assert(rect8.intersects(rect9));
    assert(rect9.intersects(rect8));
    
    std::cout << "Basic intersection detection tests passed!" << std::endl;
}

void test_intersects_edge_cases() {
    std::cout << "Testing intersection detection edge cases..." << std::endl;
    
    // Test with empty rectangles
    Rect normal(10, 10, 20, 20);
    Rect empty1(0, 10);     // Zero width
    Rect empty2(10, 0);     // Zero height
    Rect empty3(0, 0);      // Zero width and height
    
    assert(!normal.intersects(empty1));
    assert(!normal.intersects(empty2));
    assert(!normal.intersects(empty3));
    assert(!empty1.intersects(normal));
    assert(!empty2.intersects(normal));
    assert(!empty3.intersects(normal));
    assert(!empty1.intersects(empty2));
    
    // Test single pixel rectangles
    Rect pixel1(10, 10, 1, 1);
    Rect pixel2(10, 10, 1, 1);  // Same position
    Rect pixel3(11, 10, 1, 1);  // Adjacent
    Rect pixel4(9, 9, 3, 3);    // Overlapping
    
    assert(pixel1.intersects(pixel2));  // Same pixel
    assert(!pixel1.intersects(pixel3)); // Adjacent pixels
    assert(pixel1.intersects(pixel4));  // Overlapping area
    assert(pixel4.intersects(pixel1));
    
    std::cout << "Intersection detection edge cases passed!" << std::endl;
}

void test_intersection_calculation() {
    std::cout << "Testing intersection rectangle calculation..." << std::endl;
    
    // Test basic intersection
    Rect rect1(0, 0, 20, 20);
    Rect rect2(10, 10, 20, 20);
    Rect result = rect1.intersection(rect2);
    
    assert(result.x() == 10);
    assert(result.y() == 10);
    assert(result.width() == 10);
    assert(result.height() == 10);
    
    // Test symmetric intersection
    Rect result2 = rect2.intersection(rect1);
    assert(result.x() == result2.x());
    assert(result.y() == result2.y());
    assert(result.width() == result2.width());
    assert(result.height() == result2.height());
    
    // Test non-overlapping rectangles (should return empty)
    Rect rect3(0, 0, 10, 10);
    Rect rect4(20, 20, 10, 10);
    Rect empty_result = rect3.intersection(rect4);
    
    assert(empty_result.x() == 0);
    assert(empty_result.y() == 0);
    assert(empty_result.width() == 0);
    assert(empty_result.height() == 0);
    assert(empty_result.isEmpty());
    
    // Test identical rectangles
    Rect rect5(5, 5, 15, 15);
    Rect rect6(5, 5, 15, 15);
    Rect identical_result = rect5.intersection(rect6);
    
    assert(identical_result.x() == 5);
    assert(identical_result.y() == 5);
    assert(identical_result.width() == 15);
    assert(identical_result.height() == 15);
    
    std::cout << "Intersection calculation tests passed!" << std::endl;
}

void test_intersection_various_overlaps() {
    std::cout << "Testing various intersection overlap patterns..." << std::endl;
    
    // Test partial overlap from different directions
    Rect base(10, 10, 20, 20);  // Rectangle from (10,10) to (30,30)
    
    // Overlap from left
    Rect left_overlap(5, 15, 10, 10);  // (5,15) to (15,25)
    Rect left_result = base.intersection(left_overlap);
    assert(left_result.x() == 10);
    assert(left_result.y() == 15);
    assert(left_result.width() == 5);
    assert(left_result.height() == 10);
    
    // Overlap from right
    Rect right_overlap(25, 15, 10, 10);  // (25,15) to (35,25)
    Rect right_result = base.intersection(right_overlap);
    assert(right_result.x() == 25);
    assert(right_result.y() == 15);
    assert(right_result.width() == 5);
    assert(right_result.height() == 10);
    
    // Overlap from top
    Rect top_overlap(15, 5, 10, 10);  // (15,5) to (25,15)
    Rect top_result = base.intersection(top_overlap);
    assert(top_result.x() == 15);
    assert(top_result.y() == 10);
    assert(top_result.width() == 10);
    assert(top_result.height() == 5);
    
    // Overlap from bottom
    Rect bottom_overlap(15, 25, 10, 10);  // (15,25) to (25,35)
    Rect bottom_result = base.intersection(bottom_overlap);
    assert(bottom_result.x() == 15);
    assert(bottom_result.y() == 25);
    assert(bottom_result.width() == 10);
    assert(bottom_result.height() == 5);
    
    // Complete containment
    Rect inner(15, 15, 5, 5);
    Rect contain_result = base.intersection(inner);
    assert(contain_result.x() == 15);
    assert(contain_result.y() == 15);
    assert(contain_result.width() == 5);
    assert(contain_result.height() == 5);
    
    std::cout << "Various intersection overlap tests passed!" << std::endl;
}

void test_intersection_with_empty_rectangles() {
    std::cout << "Testing intersection with empty rectangles..." << std::endl;
    
    Rect normal(10, 10, 20, 20);
    Rect empty1(0, 10);     // Zero width
    Rect empty2(10, 0);     // Zero height
    Rect empty3(0, 0);      // Zero width and height
    
    // Intersection with empty rectangles should return empty
    Rect result1 = normal.intersection(empty1);
    assert(result1.isEmpty());
    
    Rect result2 = normal.intersection(empty2);
    assert(result2.isEmpty());
    
    Rect result3 = normal.intersection(empty3);
    assert(result3.isEmpty());
    
    // Symmetric test
    Rect result4 = empty1.intersection(normal);
    assert(result4.isEmpty());
    
    // Empty with empty
    Rect result5 = empty1.intersection(empty2);
    assert(result5.isEmpty());
    
    std::cout << "Empty rectangle intersection tests passed!" << std::endl;
}

void test_intersection_consistency() {
    std::cout << "Testing intersection method consistency..." << std::endl;
    
    // If intersects() returns false, intersection() should return empty rectangle
    Rect rect1(0, 0, 10, 10);
    Rect rect2(20, 20, 10, 10);
    
    assert(!rect1.intersects(rect2));
    Rect result = rect1.intersection(rect2);
    assert(result.isEmpty());
    
    // If intersects() returns true, intersection() should return non-empty rectangle
    Rect rect3(0, 0, 20, 20);
    Rect rect4(10, 10, 20, 20);
    
    assert(rect3.intersects(rect4));
    Rect result2 = rect3.intersection(rect4);
    assert(!result2.isEmpty());
    assert(result2.isValid());
    
    std::cout << "Intersection consistency tests passed!" << std::endl;
}

int main() {
    std::cout << "Running Rect intersection method tests..." << std::endl;
    
    try {
        test_intersects_basic();
        test_intersects_edge_cases();
        test_intersection_calculation();
        test_intersection_various_overlaps();
        test_intersection_with_empty_rectangles();
        test_intersection_consistency();
        
        std::cout << "All intersection tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}