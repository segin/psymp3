/*
 * test_rect_containment.cpp - Unit tests for Rect containment methods
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <cassert>
#include <iostream>
#include "../include/rect.h"

void test_point_containment() {
    std::cout << "Testing point containment..." << std::endl;
    
    // Test basic point containment
    Rect rect(10, 10, 20, 20);  // Rectangle from (10,10) to (30,30)
    
    // Points inside the rectangle
    assert(rect.contains(15, 15));  // Center area
    assert(rect.contains(10, 10));  // Top-left corner (inclusive)
    assert(rect.contains(29, 29));  // Bottom-right corner (exclusive boundary)
    
    // Points outside the rectangle
    assert(!rect.contains(5, 15));   // Left of rectangle
    assert(!rect.contains(35, 15));  // Right of rectangle
    assert(!rect.contains(15, 5));   // Above rectangle
    assert(!rect.contains(15, 35));  // Below rectangle
    assert(!rect.contains(30, 20));  // On right edge (exclusive)
    assert(!rect.contains(20, 30));  // On bottom edge (exclusive)
    
    // Test with rectangle at origin
    Rect origin_rect(0, 0, 10, 10);
    assert(origin_rect.contains(0, 0));   // Origin point
    assert(origin_rect.contains(5, 5));   // Center
    assert(origin_rect.contains(9, 9));   // Near boundary
    assert(!origin_rect.contains(10, 5)); // On right edge
    assert(!origin_rect.contains(5, 10)); // On bottom edge
    assert(!origin_rect.contains(-1, 5)); // Negative coordinates
    
    // Test with negative coordinates
    Rect neg_rect(-10, -10, 20, 20);  // Rectangle from (-10,-10) to (10,10)
    assert(neg_rect.contains(-5, -5));  // Inside
    assert(neg_rect.contains(-10, -10)); // Top-left corner
    assert(neg_rect.contains(9, 9));     // Near bottom-right
    assert(!neg_rect.contains(10, 5));   // On right edge
    assert(!neg_rect.contains(-11, 0));  // Left of rectangle
    
    std::cout << "Point containment tests passed!" << std::endl;
}

void test_point_containment_empty_rectangles() {
    std::cout << "Testing point containment with empty rectangles..." << std::endl;
    
    // Empty rectangles should not contain any points
    Rect empty1(0, 10);  // Zero width
    assert(!empty1.contains(0, 5));
    assert(!empty1.contains(5, 5));
    
    Rect empty2(10, 0);  // Zero height
    assert(!empty2.contains(5, 0));
    assert(!empty2.contains(5, 5));
    
    Rect empty3(0, 0);   // Zero width and height
    assert(!empty3.contains(0, 0));
    assert(!empty3.contains(1, 1));
    
    std::cout << "Empty rectangle point containment tests passed!" << std::endl;
}

void test_rectangle_containment() {
    std::cout << "Testing rectangle containment..." << std::endl;
    
    // Test basic rectangle containment
    Rect outer(0, 0, 100, 100);
    Rect inner(10, 10, 20, 20);
    
    assert(outer.contains(inner));   // Inner rectangle should be contained
    assert(!inner.contains(outer));  // Outer rectangle should not be contained in inner
    
    // Test identical rectangles
    Rect rect1(10, 10, 20, 20);
    Rect rect2(10, 10, 20, 20);
    assert(rect1.contains(rect2));   // Identical rectangles contain each other
    assert(rect2.contains(rect1));
    
    // Test partially overlapping rectangles
    Rect rect3(0, 0, 20, 20);
    Rect rect4(10, 10, 20, 20);
    assert(!rect3.contains(rect4));  // Partial overlap, not containment
    assert(!rect4.contains(rect3));
    
    // Test non-overlapping rectangles
    Rect rect5(0, 0, 10, 10);
    Rect rect6(20, 20, 10, 10);
    assert(!rect5.contains(rect6));
    assert(!rect6.contains(rect5));
    
    // Test edge cases - rectangles touching edges
    Rect container(0, 0, 100, 100);
    Rect edge_rect(0, 0, 100, 100);  // Same size, should contain
    assert(container.contains(edge_rect));
    
    Rect too_wide(0, 0, 101, 50);    // Extends beyond right edge
    assert(!container.contains(too_wide));
    
    Rect too_tall(0, 0, 50, 101);    // Extends beyond bottom edge
    assert(!container.contains(too_tall));
    
    std::cout << "Rectangle containment tests passed!" << std::endl;
}

void test_rectangle_containment_empty_rectangles() {
    std::cout << "Testing rectangle containment with empty rectangles..." << std::endl;
    
    Rect normal(10, 10, 20, 20);
    Rect empty1(0, 10);     // Zero width
    Rect empty2(10, 0);     // Zero height
    Rect empty3(0, 0);      // Zero width and height
    
    // Empty rectangles cannot contain anything
    assert(!empty1.contains(normal));
    assert(!empty2.contains(normal));
    assert(!empty3.contains(normal));
    assert(!empty1.contains(empty2));
    
    // Nothing can contain empty rectangles
    assert(!normal.contains(empty1));
    assert(!normal.contains(empty2));
    assert(!normal.contains(empty3));
    
    std::cout << "Empty rectangle containment tests passed!" << std::endl;
}

void test_single_pixel_rectangles() {
    std::cout << "Testing single pixel rectangle containment..." << std::endl;
    
    Rect pixel(10, 10, 1, 1);  // Single pixel at (10,10)
    
    // Point containment
    assert(pixel.contains(10, 10));   // The single pixel
    assert(!pixel.contains(11, 10));  // Adjacent pixels
    assert(!pixel.contains(10, 11));
    assert(!pixel.contains(9, 10));
    assert(!pixel.contains(10, 9));
    
    // Rectangle containment
    Rect container(5, 5, 10, 10);
    assert(container.contains(pixel));  // Container should contain single pixel
    assert(!pixel.contains(container)); // Single pixel cannot contain larger rectangle
    
    Rect another_pixel(10, 10, 1, 1);
    assert(pixel.contains(another_pixel)); // Identical single pixels
    
    std::cout << "Single pixel rectangle tests passed!" << std::endl;
}

int main() {
    std::cout << "Running Rect containment method tests..." << std::endl;
    
    try {
        test_point_containment();
        test_point_containment_empty_rectangles();
        test_rectangle_containment();
        test_rectangle_containment_empty_rectangles();
        test_single_pixel_rectangles();
        
        std::cout << "All containment tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}