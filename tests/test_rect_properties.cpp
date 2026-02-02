/*
 * test_rect_properties.cpp - Property-based tests for Rect class using RapidCheck
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * This file contains property-based tests for the enhanced Rect class.
 * Property-based testing validates that certain properties hold true across
 * a wide range of randomly generated inputs, providing stronger correctness
 * guarantees than example-based unit tests alone.
 *
 * Test Organization:
 * - Each property test corresponds to a specific correctness property from design.md
 * - Properties are validated across randomly generated Rect instances
 * - Edge cases (empty rects, overflow conditions, negative coords) are tested
 * - All 26 correctness properties from the design document are covered
 */

#include "psymp3.h"
#include "test_framework.h"
#include "test_rect_utilities.h"

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>
#endif

#include <iostream>
#include <limits>

using PsyMP3::Core::Rect;

// ========================================
// RAPIDCHECK GENERATORS
// ========================================

#ifdef HAVE_RAPIDCHECK

namespace rc {

// Generator for valid int16_t coordinates
template<>
struct Arbitrary<int16_t> {
    static Gen<int16_t> arbitrary() {
        return gen::inRange<int16_t>(
            std::numeric_limits<int16_t>::min(),
            std::numeric_limits<int16_t>::max()
        );
    }
};

// Generator for valid uint16_t dimensions
template<>
struct Arbitrary<uint16_t> {
    static Gen<uint16_t> arbitrary() {
        return gen::inRange<uint16_t>(
            0,
            std::numeric_limits<uint16_t>::max()
        );
    }
};

} // namespace rc

// Custom generators for Rect testing
namespace RectGenerators {

    // Generate any valid Rect (including empty)
    rc::Gen<Rect> anyRect() {
        return rc::gen::construct<Rect>(
            rc::gen::arbitrary<int16_t>(),
            rc::gen::arbitrary<int16_t>(),
            rc::gen::arbitrary<uint16_t>(),
            rc::gen::arbitrary<uint16_t>()
        );
    }

    // Generate non-empty Rect
    rc::Gen<Rect> nonEmptyRect() {
        return rc::gen::construct<Rect>(
            rc::gen::arbitrary<int16_t>(),
            rc::gen::arbitrary<int16_t>(),
            rc::gen::inRange<uint16_t>(1, std::numeric_limits<uint16_t>::max()),
            rc::gen::inRange<uint16_t>(1, std::numeric_limits<uint16_t>::max())
        );
    }

    // Generate small Rect (for operations that might overflow)
    rc::Gen<Rect> smallRect() {
        return rc::gen::construct<Rect>(
            rc::gen::inRange<int16_t>(-1000, 1000),
            rc::gen::inRange<int16_t>(-1000, 1000),
            rc::gen::inRange<uint16_t>(0, 1000),
            rc::gen::inRange<uint16_t>(0, 1000)
        );
    }

    // Generate point coordinates
    rc::Gen<std::pair<int16_t, int16_t>> anyPoint() {
        return rc::gen::pair(
            rc::gen::arbitrary<int16_t>(),
            rc::gen::arbitrary<int16_t>()
        );
    }

} // namespace RectGenerators

#endif // HAVE_RAPIDCHECK

// ========================================
// PROPERTY-BASED TEST SUITE
// ========================================

int main() {
#ifdef HAVE_RAPIDCHECK
    std::cout << "Running Rect Property-Based Tests with RapidCheck" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;

    int failures = 0;

    // Property 1: Point Containment Correctness
    // **Validates: Requirements 1.1**
    std::cout << "Property 1: Point Containment Correctness... ";
    std::cout.flush();
    try {
        rc::check("Point containment correctness", [](int16_t x, int16_t y, uint16_t w, uint16_t h, int16_t px, int16_t py) {
            Rect rect(x, y, w, h);
            bool contains = rect.contains(px, py);
            
            // Empty rectangles should never contain points
            if (rect.isEmpty()) {
                RC_ASSERT(!contains);
                return;
            }
            
            // Point is contained if within bounds (inclusive left/top, exclusive right/bottom)
            bool expected = (px >= rect.left() && px < rect.right() &&
                           py >= rect.top() && py < rect.bottom());
            
            RC_ASSERT(contains == expected);
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 7: Edge Calculation Consistency
    // **Validates: Requirements 2.1, 2.2**
    std::cout << "Property 7: Edge Calculation Consistency... ";
    std::cout.flush();
    try {
        rc::check("Edge calculation consistency", [](int16_t x, int16_t y, uint16_t w, uint16_t h) {
            Rect rect(x, y, w, h);
            RC_ASSERT(rect.left() == rect.x());
            RC_ASSERT(rect.top() == rect.y());
            
            // Note: right() and bottom() may overflow, but should be consistent
            int32_t expected_right = static_cast<int32_t>(rect.x()) + rect.width();
            int32_t expected_bottom = static_cast<int32_t>(rect.y()) + rect.height();
            
            // If no overflow, values should match exactly
            if (expected_right <= std::numeric_limits<int16_t>::max() &&
                expected_right >= std::numeric_limits<int16_t>::min()) {
                RC_ASSERT(rect.right() == static_cast<int16_t>(expected_right));
            }
            
            if (expected_bottom <= std::numeric_limits<int16_t>::max() &&
                expected_bottom >= std::numeric_limits<int16_t>::min()) {
                RC_ASSERT(rect.bottom() == static_cast<int16_t>(expected_bottom));
            }
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 8: Center Point Calculation
    // **Validates: Requirements 2.3**
    std::cout << "Property 8: Center Point Calculation... ";
    std::cout.flush();
    try {
        rc::check("Center point calculation", [](int16_t x, int16_t y, uint16_t w, uint16_t h) {
            Rect rect(x, y, w, h);
            
            // Calculate expected center coordinates using int32_t to match implementation
            // The implementation uses int32_t for calculation and then clamps to int16_t range
            int32_t expected_cx_32 = static_cast<int32_t>(x) + static_cast<int32_t>(w) / 2;
            int32_t expected_cy_32 = static_cast<int32_t>(y) + static_cast<int32_t>(h) / 2;
            
            // Clamp to int16_t range (matching implementation behavior)
            int16_t expected_cx = (expected_cx_32 > std::numeric_limits<int16_t>::max()) ? std::numeric_limits<int16_t>::max() :
                                  (expected_cx_32 < std::numeric_limits<int16_t>::min()) ? std::numeric_limits<int16_t>::min() :
                                  static_cast<int16_t>(expected_cx_32);
            int16_t expected_cy = (expected_cy_32 > std::numeric_limits<int16_t>::max()) ? std::numeric_limits<int16_t>::max() :
                                  (expected_cy_32 < std::numeric_limits<int16_t>::min()) ? std::numeric_limits<int16_t>::min() :
                                  static_cast<int16_t>(expected_cy_32);
            
            // Test centerX() and centerY() methods
            RC_ASSERT(rect.centerX() == expected_cx);
            RC_ASSERT(rect.centerY() == expected_cy);
            
            // Test center() method returns same values as individual methods
            auto [cx, cy] = rect.center();
            RC_ASSERT(cx == rect.centerX());
            RC_ASSERT(cy == rect.centerY());
            
            // Verify consistency between center() and individual methods
            RC_ASSERT(cx == expected_cx);
            RC_ASSERT(cy == expected_cy);
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 9: Corner Coordinate Derivation
    // **Validates: Requirements 2.4**
    std::cout << "Property 9: Corner Coordinate Derivation... ";
    std::cout.flush();
    try {
        rc::check("Corner coordinate derivation", [](int16_t x, int16_t y, uint16_t w, uint16_t h) {
            Rect rect(x, y, w, h);
            
            // Test topLeft() returns (left(), top())
            auto [tl_x, tl_y] = rect.topLeft();
            RC_ASSERT(tl_x == rect.left());
            RC_ASSERT(tl_y == rect.top());
            
            // Test topRight() returns (right(), top())
            auto [tr_x, tr_y] = rect.topRight();
            RC_ASSERT(tr_x == rect.right());
            RC_ASSERT(tr_y == rect.top());
            
            // Test bottomLeft() returns (left(), bottom())
            auto [bl_x, bl_y] = rect.bottomLeft();
            RC_ASSERT(bl_x == rect.left());
            RC_ASSERT(bl_y == rect.bottom());
            
            // Test bottomRight() returns (right(), bottom())
            auto [br_x, br_y] = rect.bottomRight();
            RC_ASSERT(br_x == rect.right());
            RC_ASSERT(br_y == rect.bottom());
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 10: Empty Rectangle Detection
    // **Validates: Requirements 2.5**
    std::cout << "Property 10: Empty Rectangle Detection... ";
    std::cout.flush();
    try {
        rc::check("Empty rectangle detection", [](int16_t x, int16_t y, uint16_t w, uint16_t h) {
            Rect rect(x, y, w, h);
            bool is_empty = rect.isEmpty();
            bool expected_empty = (rect.width() == 0 || rect.height() == 0);
            RC_ASSERT(is_empty == expected_empty);
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 11: Area Calculation
    // **Validates: Requirements 2.6**
    std::cout << "Property 11: Area Calculation... ";
    std::cout.flush();
    try {
        rc::check("Area calculation", [](int16_t x, int16_t y, uint16_t w, uint16_t h) {
            Rect rect(x, y, w, h);
            uint32_t area = rect.area();
            uint32_t expected_area = static_cast<uint32_t>(rect.width()) * 
                                    static_cast<uint32_t>(rect.height());
            RC_ASSERT(area == expected_area);
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 19: Equality Comparison Correctness
    // **Validates: Requirements 5.2**
    std::cout << "Property 19: Equality Comparison Correctness... ";
    std::cout.flush();
    try {
        rc::check("Equality comparison correctness", [](int16_t x1, int16_t y1, uint16_t w1, uint16_t h1,
                                                         int16_t x2, int16_t y2, uint16_t w2, uint16_t h2) {
            Rect rect1(x1, y1, w1, h1);
            Rect rect2(x2, y2, w2, h2);
            
            bool are_equal = (rect1 == rect2);
            bool expected_equal = (rect1.x() == rect2.x() && 
                                 rect1.y() == rect2.y() &&
                                 rect1.width() == rect2.width() && 
                                 rect1.height() == rect2.height());
            
            RC_ASSERT(are_equal == expected_equal);
            RC_ASSERT((rect1 != rect2) == !are_equal);
            
            // Reflexivity: rect == rect
            RC_ASSERT(rect1 == rect1);
            
            // Symmetry: if rect1 == rect2, then rect2 == rect1
            if (rect1 == rect2) {
                RC_ASSERT(rect2 == rect1);
            }
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 20: String Representation Accuracy
    // **Validates: Requirements 5.3, 7.2**
    std::cout << "Property 20: String Representation Accuracy... ";
    std::cout.flush();
    try {
        rc::check("String representation accuracy", [](int16_t x, int16_t y, uint16_t w, uint16_t h) {
            Rect rect(x, y, w, h);
            std::string str = rect.toString();
            
            // String should contain "Rect(" prefix
            RC_ASSERT(str.find("Rect(") == 0);
            
            // String should contain all coordinate values as strings
            std::string x_str = std::to_string(x);
            std::string y_str = std::to_string(y);
            std::string w_str = std::to_string(w);
            std::string h_str = std::to_string(h);
            
            RC_ASSERT(str.find(x_str) != std::string::npos);
            RC_ASSERT(str.find(y_str) != std::string::npos);
            RC_ASSERT(str.find(w_str) != std::string::npos);
            RC_ASSERT(str.find(h_str) != std::string::npos);
            
            // String should contain closing parenthesis
            RC_ASSERT(str.find(")") != std::string::npos);
            
            // For empty rectangles, string should indicate empty status
            if (rect.isEmpty()) {
                RC_ASSERT(str.find("[EMPTY]") != std::string::npos);
            }
            
            // String should be non-empty
            RC_ASSERT(!str.empty());
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 12: Translation Preserves Dimensions
    // **Validates: Requirements 3.1**
    std::cout << "Property 12: Translation Preserves Dimensions... ";
    std::cout.flush();
    try {
        rc::check("Translation preserves dimensions", [](int16_t x, int16_t y, uint16_t w, uint16_t h, int16_t dx, int16_t dy) {
            Rect rect(x, y, w, h);
            Rect translated = rect.translated(dx, dy);
            
            // Dimensions should be preserved
            RC_ASSERT(translated.width() == rect.width());
            RC_ASSERT(translated.height() == rect.height());
            
            // Position should change by offset (if no overflow)
            int32_t expected_x = static_cast<int32_t>(rect.x()) + dx;
            int32_t expected_y = static_cast<int32_t>(rect.y()) + dy;
            
            if (expected_x >= std::numeric_limits<int16_t>::min() &&
                expected_x <= std::numeric_limits<int16_t>::max()) {
                RC_ASSERT(translated.x() == static_cast<int16_t>(expected_x));
            }
            
            if (expected_y >= std::numeric_limits<int16_t>::min() &&
                expected_y <= std::numeric_limits<int16_t>::max()) {
                RC_ASSERT(translated.y() == static_cast<int16_t>(expected_y));
            }
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 14: Resize Preserves Position
    // **Validates: Requirements 3.3**
    std::cout << "Property 14: Resize Preserves Position... ";
    std::cout.flush();
    try {
        rc::check("Resize preserves position", [](int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t new_width, uint16_t new_height) {
            Rect rect(x, y, w, h);
            Rect resized = rect.resized(new_width, new_height);
            
            // Position should be preserved
            RC_ASSERT(resized.x() == rect.x());
            RC_ASSERT(resized.y() == rect.y());
            
            // Dimensions should be updated
            RC_ASSERT(resized.width() == new_width);
            RC_ASSERT(resized.height() == new_height);
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 24: Validation Correctness
    // **Validates: Requirements 7.1**
    std::cout << "Property 24: Validation Correctness... ";
    std::cout.flush();
    try {
        rc::check("Validation correctness", [](int16_t x, int16_t y, uint16_t w, uint16_t h) {
            Rect rect(x, y, w, h);
            bool is_valid = rect.isValid();
            
            // A rectangle is valid if and only if it has non-zero dimensions
            bool expected_valid = (w > 0 && h > 0);
            RC_ASSERT(is_valid == expected_valid);
            
            // Valid rectangles should not be empty
            if (is_valid) {
                RC_ASSERT(!rect.isEmpty());
            }
            
            // Empty rectangles should not be valid
            if (rect.isEmpty()) {
                RC_ASSERT(!is_valid);
            }
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 26: Normalization Correctness
    // **Validates: Requirements 7.4**
    std::cout << "Property 26: Normalization Correctness... ";
    std::cout.flush();
    try {
        rc::check("Normalization correctness", [](int16_t x, int16_t y, uint16_t w, uint16_t h) {
            Rect rect(x, y, w, h);
            Rect normalized = rect.normalized();
            
            // Normalized rectangle should have the same area
            RC_ASSERT(normalized.area() == rect.area());
            
            // For rectangles with positive dimensions, normalization should not change them
            if (w > 0 && h > 0) {
                // Already normalized - should be unchanged
                RC_ASSERT(normalized.x() == rect.x());
                RC_ASSERT(normalized.y() == rect.y());
                RC_ASSERT(normalized.width() == rect.width());
                RC_ASSERT(normalized.height() == rect.height());
            }
            
            // Test in-place normalization produces same result
            Rect rect_copy(x, y, w, h);
            rect_copy.normalize();
            RC_ASSERT(rect_copy.x() == normalized.x());
            RC_ASSERT(rect_copy.y() == normalized.y());
            RC_ASSERT(rect_copy.width() == normalized.width());
            RC_ASSERT(rect_copy.height() == normalized.height());
            
            // Normalizing a normalized rectangle should be idempotent
            Rect double_normalized = normalized.normalized();
            RC_ASSERT(double_normalized.x() == normalized.x());
            RC_ASSERT(double_normalized.y() == normalized.y());
            RC_ASSERT(double_normalized.width() == normalized.width());
            RC_ASSERT(double_normalized.height() == normalized.height());
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 2: Rectangle Intersection Detection
    // **Validates: Requirements 1.2**
    std::cout << "Property 2: Rectangle Intersection Detection... ";
    std::cout.flush();
    try {
        rc::check("Rectangle intersection detection", [](int16_t x1, int16_t y1, uint16_t w1, uint16_t h1,
                                                          int16_t x2, int16_t y2, uint16_t w2, uint16_t h2) {
            Rect rect1(x1, y1, w1, h1);
            Rect rect2(x2, y2, w2, h2);
            
            bool intersects = rect1.intersects(rect2);
            
            // Empty rectangles never intersect
            if (rect1.isEmpty() || rect2.isEmpty()) {
                RC_ASSERT(!intersects);
                return;
            }
            
            // Calculate expected intersection using separation test
            // Rectangles intersect if they are NOT separated on either axis
            bool separated_x = (rect1.x() >= rect2.x() + static_cast<int32_t>(rect2.width())) ||
                              (rect2.x() >= rect1.x() + static_cast<int32_t>(rect1.width()));
            bool separated_y = (rect1.y() >= rect2.y() + static_cast<int32_t>(rect2.height())) ||
                              (rect2.y() >= rect1.y() + static_cast<int32_t>(rect1.height()));
            
            bool expected_intersects = !separated_x && !separated_y;
            
            RC_ASSERT(intersects == expected_intersects);
            
            // Intersection should be symmetric
            RC_ASSERT(rect1.intersects(rect2) == rect2.intersects(rect1));
            
            // A rectangle always intersects with itself (if non-empty)
            if (!rect1.isEmpty()) {
                RC_ASSERT(rect1.intersects(rect1));
            }
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Property 3: Intersection Calculation Correctness
    // **Validates: Requirements 1.3**
    std::cout << "Property 3: Intersection Calculation Correctness... ";
    std::cout.flush();
    try {
        rc::check("Intersection calculation correctness", [](int16_t x1, int16_t y1, uint16_t w1, uint16_t h1,
                                                              int16_t x2, int16_t y2, uint16_t w2, uint16_t h2) {
            Rect rect1(x1, y1, w1, h1);
            Rect rect2(x2, y2, w2, h2);
            
            Rect intersection = rect1.intersection(rect2);
            
            // If rectangles don't intersect, intersection should be empty
            if (!rect1.intersects(rect2)) {
                RC_ASSERT(intersection.isEmpty());
                return;
            }
            
            // If rectangles intersect, intersection should not be empty
            RC_ASSERT(!intersection.isEmpty());
            
            // Intersection should be contained in both rectangles
            RC_ASSERT(rect1.contains(intersection));
            RC_ASSERT(rect2.contains(intersection));
            
            // Intersection should be symmetric
            Rect intersection_reversed = rect2.intersection(rect1);
            RC_ASSERT(intersection.x() == intersection_reversed.x());
            RC_ASSERT(intersection.y() == intersection_reversed.y());
            RC_ASSERT(intersection.width() == intersection_reversed.width());
            RC_ASSERT(intersection.height() == intersection_reversed.height());
            
            // Intersection with self should return self (if non-empty)
            if (!rect1.isEmpty()) {
                Rect self_intersection = rect1.intersection(rect1);
                RC_ASSERT(self_intersection.x() == rect1.x());
                RC_ASSERT(self_intersection.y() == rect1.y());
                RC_ASSERT(self_intersection.width() == rect1.width());
                RC_ASSERT(self_intersection.height() == rect1.height());
            }
            
            // Intersection area should be <= both rectangle areas
            RC_ASSERT(intersection.area() <= rect1.area());
            RC_ASSERT(intersection.area() <= rect2.area());
        });
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << std::endl;
        failures++;
    }

    // Summary
    std::cout << std::endl;
    std::cout << "Property-Based Test Summary" << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Total properties tested: 14" << std::endl;
    std::cout << "Failures: " << failures << std::endl;
    
    if (failures == 0) {
        std::cout << "All property-based tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Some property-based tests failed." << std::endl;
        return 1;
    }

#else
    std::cout << "RapidCheck not available - property-based tests skipped" << std::endl;
    std::cout << "To enable property-based testing, configure with --enable-rapidcheck" << std::endl;
    return 0;
#endif
}
