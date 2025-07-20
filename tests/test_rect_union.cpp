/*
 * test_rect_union.cpp - Unit tests for Rect union method
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <cassert>
#include <iostream>
#include "../include/rect.h"

void test_union_basic() {
    std::cout << "Testing basic rectangle union..." << std::endl;
    
    // Test union of overlapping rectangles
    Rect rect1(0, 0, 20, 20);
    Rect rect2(10, 10, 20, 20);
    Rect result = rect1.united(rect2);
    
    assert(result.x() == 0);
    assert(result.y() == 0);
    assert(result.width() == 30);
    assert(result.height() == 30);
    
    // Test symmetric union
    Rect result2 = rect2.united(rect1);
    assert(result.x() == result2.x());
    assert(result.y() == result2.y());
    assert(result.width() == result2.width());
    assert(result.height() == result2.height());
    
    // Test union of non-overlapping rectangles
    Rect rect3(0, 0, 10, 10);
    Rect rect4(20, 20, 10, 10);
    Rect result3 = rect3.united(rect4);
    
    assert(result3.x() == 0);
    assert(result3.y() == 0);
    assert(result3.width() == 30);
    assert(result3.height() == 30);
    
    // Test union of identical rectangles
    Rect rect5(5, 5, 15, 15);
    Rect rect6(5, 5, 15, 15);
    Rect result4 = rect5.united(rect6);
    
    assert(result4.x() == 5);
    assert(result4.y() == 5);
    assert(result4.width() == 15);
    assert(result4.height() == 15);
    
    std::cout << "Basic union tests passed!" << std::endl;
}

void test_union_containment() {
    std::cout << "Testing union with containment scenarios..." << std::endl;
    
    // Test union where one rectangle contains the other
    Rect outer(0, 0, 100, 100);
    Rect inner(10, 10, 20, 20);
    
    Rect result1 = outer.united(inner);
    assert(result1.x() == 0);
    assert(result1.y() == 0);
    assert(result1.width() == 100);
    assert(result1.height() == 100);
    
    Rect result2 = inner.united(outer);
    assert(result2.x() == 0);
    assert(result2.y() == 0);
    assert(result2.width() == 100);
    assert(result2.height() == 100);
    
    std::cout << "Union containment tests passed!" << std::endl;
}

void test_union_with_empty_rectangles() {
    std::cout << "Testing union with empty rectangles..." << std::endl;
    
    Rect normal(10, 10, 20, 20);
    Rect empty1(0, 10);     // Zero width
    Rect empty2(10, 0);     // Zero height
    Rect empty3(0, 0);      // Zero width and height
    
    // Union with empty rectangle should return the non-empty rectangle
    Rect result1 = normal.united(empty1);
    assert(result1.x() == normal.x());
    assert(result1.y() == normal.y());
    assert(result1.width() == normal.width());
    assert(result1.height() == normal.height());
    
    Rect result2 = normal.united(empty2);
    assert(result2.x() == normal.x());
    assert(result2.y() == normal.y());
    assert(result2.width() == normal.width());
    assert(result2.height() == normal.height());
    
    Rect result3 = normal.united(empty3);
    assert(result3.x() == normal.x());
    assert(result3.y() == normal.y());
    assert(result3.width() == normal.width());
    assert(result3.height() == normal.height());
    
    // Symmetric test
    Rect result4 = empty1.united(normal);
    assert(result4.x() == normal.x());
    assert(result4.y() == normal.y());
    assert(result4.width() == normal.width());
    assert(result4.height() == normal.height());
    
    // Union of two empty rectangles should return empty
    Rect result5 = empty1.united(empty2);
    assert(result5.isEmpty());
    
    Rect result6 = empty3.united(empty3);
    assert(result6.isEmpty());
    
    std::cout << "Empty rectangle union tests passed!" << std::endl;
}

void test_union_various_positions() {
    std::cout << "Testing union with various rectangle positions..." << std::endl;
    
    Rect base(10, 10, 10, 10);  // Rectangle from (10,10) to (20,20)
    
    // Union with rectangle to the left
    Rect left(0, 10, 10, 10);   // (0,10) to (10,20)
    Rect left_result = base.united(left);
    assert(left_result.x() == 0);
    assert(left_result.y() == 10);
    assert(left_result.width() == 20);
    assert(left_result.height() == 10);
    
    // Union with rectangle to the right
    Rect right(20, 10, 10, 10); // (20,10) to (30,20)
    Rect right_result = base.united(right);
    assert(right_result.x() == 10);
    assert(right_result.y() == 10);
    assert(right_result.width() == 20);
    assert(right_result.height() == 10);
    
    // Union with rectangle above
    Rect above(10, 0, 10, 10);  // (10,0) to (20,10)
    Rect above_result = base.united(above);
    assert(above_result.x() == 10);
    assert(above_result.y() == 0);
    assert(above_result.width() == 10);
    assert(above_result.height() == 20);
    
    // Union with rectangle below
    Rect below(10, 20, 10, 10); // (10,20) to (20,30)
    Rect below_result = base.united(below);
    assert(below_result.x() == 10);
    assert(below_result.y() == 10);
    assert(below_result.width() == 10);
    assert(below_result.height() == 20);
    
    // Union with diagonal rectangle
    Rect diagonal(0, 0, 5, 5);  // (0,0) to (5,5)
    Rect diag_result = base.united(diagonal);
    assert(diag_result.x() == 0);
    assert(diag_result.y() == 0);
    assert(diag_result.width() == 20);
    assert(diag_result.height() == 20);
    
    std::cout << "Various position union tests passed!" << std::endl;
}

void test_union_negative_coordinates() {
    std::cout << "Testing union with negative coordinates..." << std::endl;
    
    // Test union with negative coordinates
    Rect pos(10, 10, 10, 10);   // (10,10) to (20,20)
    Rect neg(-10, -10, 10, 10); // (-10,-10) to (0,0)
    
    Rect result = pos.united(neg);
    assert(result.x() == -10);
    assert(result.y() == -10);
    assert(result.width() == 30);
    assert(result.height() == 30);
    
    // Test union spanning across zero
    Rect span1(-5, -5, 10, 10); // (-5,-5) to (5,5)
    Rect span2(5, 5, 10, 10);   // (5,5) to (15,15)
    
    Rect span_result = span1.united(span2);
    assert(span_result.x() == -5);
    assert(span_result.y() == -5);
    assert(span_result.width() == 20);
    assert(span_result.height() == 20);
    
    std::cout << "Negative coordinate union tests passed!" << std::endl;
}

void test_union_overflow_handling() {
    std::cout << "Testing union coordinate overflow handling..." << std::endl;
    
    // Test potential overflow scenarios
    // Create rectangles that when united might cause width/height overflow
    Rect rect1(0, 0, 32767, 32767);        // Large rectangle from (0,0) to (32767,32767)
    Rect rect2(32767, 32767, 32767, 32767); // Another large rectangle from (32767,32767) to (65534,65534)
    
    Rect result = rect1.united(rect2);
    
    // The result should handle overflow gracefully
    assert(result.x() == 0);
    assert(result.y() == 0);
    // The union spans from (0,0) to (65534,65534), so width and height should be 65534
    assert(result.width() == 65534);
    assert(result.height() == 65534);
    
    // Test with maximum uint16_t values that would cause overflow in calculation
    // We need to be careful with uint16_t limits (0-65535)
    Rect max1(-10000, -10000, 65535, 65535);  // Large rectangle at negative position
    Rect max2(10000, 10000, 65535, 65535);    // Large rectangle at positive position
    
    Rect max_result = max1.united(max2);
    assert(max_result.x() == -10000);
    assert(max_result.y() == -10000);
    // The calculation would be: right = max(55535, 75535) = 75535, left = -10000
    // So width = 75535 - (-10000) = 85535, which should be clamped to 65535
    assert(max_result.width() == 65535);  // Should be clamped due to overflow
    assert(max_result.height() == 65535); // Should be clamped due to overflow
    
    std::cout << "Overflow handling tests passed!" << std::endl;
}

void test_union_single_pixel() {
    std::cout << "Testing union with single pixel rectangles..." << std::endl;
    
    // Test union of single pixel rectangles
    Rect pixel1(10, 10, 1, 1);
    Rect pixel2(12, 12, 1, 1);
    
    Rect result = pixel1.united(pixel2);
    assert(result.x() == 10);
    assert(result.y() == 10);
    assert(result.width() == 3);
    assert(result.height() == 3);
    
    // Test union of single pixel with larger rectangle
    Rect large(0, 0, 20, 20);
    Rect pixel(25, 25, 1, 1);
    
    Rect result2 = large.united(pixel);
    assert(result2.x() == 0);
    assert(result2.y() == 0);
    assert(result2.width() == 26);
    assert(result2.height() == 26);
    
    std::cout << "Single pixel union tests passed!" << std::endl;
}

int main() {
    std::cout << "Running Rect union method tests..." << std::endl;
    
    try {
        test_union_basic();
        test_union_containment();
        test_union_with_empty_rectangles();
        test_union_various_positions();
        test_union_negative_coordinates();
        test_union_overflow_handling();
        test_union_single_pixel();
        
        std::cout << "All union tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}