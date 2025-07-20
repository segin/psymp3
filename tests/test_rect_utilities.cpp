/*
 * test_rect_utilities.cpp - Implementation of common utilities for Rect testing
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "test_rect_utilities.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace RectTestUtilities {

    // ========================================
    // COMMON RECT TEST SCENARIOS
    // ========================================
    
    namespace TestRects {
        
        Rect standard() {
            return Rect(10, 20, 100, 50);
        }
        
        Rect empty() {
            return Rect(0, 0, 0, 0);
        }
        
        Rect zeroWidth() {
            return Rect(10, 10, 0, 20);
        }
        
        Rect zeroHeight() {
            return Rect(10, 10, 20, 0);
        }
        
        Rect singlePixel() {
            return Rect(5, 5, 1, 1);
        }
        
        Rect atOrigin() {
            return Rect(0, 0, 50, 50);
        }
        
        Rect withNegativeCoords() {
            return Rect(-10, -10, 20, 20);
        }
        
        Rect large() {
            return Rect(0, 0, 65535, 65535);
        }
        
        Rect container() {
            return Rect(0, 0, 100, 100);
        }
        
        Rect offsetContainer() {
            return Rect(20, 30, 60, 40);
        }
    }

    // ========================================
    // ENHANCED ASSERTION UTILITIES
    // ========================================
    
    void assertRectEquals(const Rect& rect, int16_t x, int16_t y, uint16_t width, uint16_t height, const std::string& message) {
        bool matches = (rect.x() == x && rect.y() == y && rect.width() == width && rect.height() == height);
        
        if (!matches) {
            std::ostringstream oss;
            oss << "Rectangle mismatch: " << message 
                << " - Expected: (" << x << ", " << y << ", " << width << ", " << height << ")"
                << ", Got: (" << rect.x() << ", " << rect.y() << ", " << rect.width() << ", " << rect.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
    }
    
    void assertRectsIdentical(const Rect& expected, const Rect& actual, const std::string& message) {
        if (!(expected == actual)) {
            std::ostringstream oss;
            oss << "Rectangles not identical: " << message 
                << " - Expected: (" << expected.x() << ", " << expected.y() << ", " << expected.width() << ", " << expected.height() << ")"
                << ", Got: (" << actual.x() << ", " << actual.y() << ", " << actual.width() << ", " << actual.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
    }
    
    void assertRectEmpty(const Rect& rect, const std::string& message) {
        if (!rect.isEmpty()) {
            std::ostringstream oss;
            oss << "Rectangle should be empty: " << message 
                << " - Got: (" << rect.x() << ", " << rect.y() << ", " << rect.width() << ", " << rect.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
    }
    
    void assertRectNotEmpty(const Rect& rect, const std::string& message) {
        if (rect.isEmpty()) {
            std::ostringstream oss;
            oss << "Rectangle should not be empty: " << message 
                << " - Got: (" << rect.x() << ", " << rect.y() << ", " << rect.width() << ", " << rect.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
    }
    
    void assertRectValid(const Rect& rect, const std::string& message) {
        if (!rect.isValid()) {
            std::ostringstream oss;
            oss << "Rectangle should be valid: " << message 
                << " - Got: (" << rect.x() << ", " << rect.y() << ", " << rect.width() << ", " << rect.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
    }
    
    void assertRectInvalid(const Rect& rect, const std::string& message) {
        if (rect.isValid()) {
            std::ostringstream oss;
            oss << "Rectangle should be invalid: " << message 
                << " - Got: (" << rect.x() << ", " << rect.y() << ", " << rect.width() << ", " << rect.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
    }
    
    void assertRectArea(const Rect& rect, uint32_t expected_area, const std::string& message) {
        uint32_t actual_area = rect.area();
        if (actual_area != expected_area) {
            std::ostringstream oss;
            oss << "Rectangle area mismatch: " << message 
                << " - Expected: " << expected_area << ", Got: " << actual_area
                << " (Rectangle: " << rect.x() << ", " << rect.y() << ", " << rect.width() << ", " << rect.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
    }
    
    void assertRectCenter(const Rect& rect, int16_t expected_center_x, int16_t expected_center_y, const std::string& message) {
        int16_t actual_center_x = rect.centerX();
        int16_t actual_center_y = rect.centerY();
        
        if (actual_center_x != expected_center_x || actual_center_y != expected_center_y) {
            std::ostringstream oss;
            oss << "Rectangle center mismatch: " << message 
                << " - Expected center: (" << expected_center_x << ", " << expected_center_y << ")"
                << ", Got center: (" << actual_center_x << ", " << actual_center_y << ")"
                << " (Rectangle: " << rect.x() << ", " << rect.y() << ", " << rect.width() << ", " << rect.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
    }

    // ========================================
    // COMMON TEST PATTERNS
    // ========================================
    
    void testBasicProperties(const Rect& rect, uint32_t expected_area, bool should_be_empty, bool should_be_valid, const std::string& test_context) {
        // Test area calculation
        assertRectArea(rect, expected_area, test_context + " - area calculation");
        
        // Test isEmpty
        if (should_be_empty) {
            assertRectEmpty(rect, test_context + " - should be empty");
        } else {
            assertRectNotEmpty(rect, test_context + " - should not be empty");
        }
        
        // Test isValid
        if (should_be_valid) {
            assertRectValid(rect, test_context + " - should be valid");
        } else {
            assertRectInvalid(rect, test_context + " - should be invalid");
        }
        
        // Test consistency between isEmpty and isValid for zero-dimension rectangles
        if (rect.width() == 0 || rect.height() == 0) {
            assertRectEmpty(rect, test_context + " - zero dimension should be empty");
            assertRectInvalid(rect, test_context + " - zero dimension should be invalid");
        }
    }
    
    void testContainmentPatterns(const Rect& container, const Rect& inner, bool should_contain, const std::string& test_context) {
        bool actually_contains = container.contains(inner);
        
        if (should_contain && !actually_contains) {
            std::ostringstream oss;
            oss << "Container should contain inner rectangle: " << test_context
                << " - Container: (" << container.x() << ", " << container.y() << ", " << container.width() << ", " << container.height() << ")"
                << ", Inner: (" << inner.x() << ", " << inner.y() << ", " << inner.width() << ", " << inner.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
        
        if (!should_contain && actually_contains) {
            std::ostringstream oss;
            oss << "Container should not contain inner rectangle: " << test_context
                << " - Container: (" << container.x() << ", " << container.y() << ", " << container.width() << ", " << container.height() << ")"
                << ", Inner: (" << inner.x() << ", " << inner.y() << ", " << inner.width() << ", " << inner.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
    }
    
    void testIntersectionPatterns(const Rect& rect1, const Rect& rect2, bool should_intersect, const Rect& expected_intersection, const std::string& test_context) {
        bool actually_intersects = rect1.intersects(rect2);
        
        // Test intersects() method
        if (should_intersect && !actually_intersects) {
            std::ostringstream oss;
            oss << "Rectangles should intersect: " << test_context
                << " - Rect1: (" << rect1.x() << ", " << rect1.y() << ", " << rect1.width() << ", " << rect1.height() << ")"
                << ", Rect2: (" << rect2.x() << ", " << rect2.y() << ", " << rect2.width() << ", " << rect2.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
        
        if (!should_intersect && actually_intersects) {
            std::ostringstream oss;
            oss << "Rectangles should not intersect: " << test_context
                << " - Rect1: (" << rect1.x() << ", " << rect1.y() << ", " << rect1.width() << ", " << rect1.height() << ")"
                << ", Rect2: (" << rect2.x() << ", " << rect2.y() << ", " << rect2.width() << ", " << rect2.height() << ")";
            throw TestFramework::AssertionFailure(oss.str());
        }
        
        // Test intersection() method
        Rect actual_intersection = rect1.intersection(rect2);
        
        if (should_intersect) {
            assertRectsIdentical(expected_intersection, actual_intersection, test_context + " - intersection result");
        } else {
            assertRectEmpty(actual_intersection, test_context + " - non-intersecting should return empty");
        }
        
        // Test symmetry
        RECT_ASSERT_EQUALS(rect1.intersects(rect2), rect2.intersects(rect1), test_context + " - intersection symmetry");
    }
    
    void testUnionPatterns(const Rect& rect1, const Rect& rect2, const Rect& expected_union, const std::string& test_context) {
        Rect actual_union = rect1.united(rect2);
        assertRectsIdentical(expected_union, actual_union, test_context + " - union result");
        
        // Test symmetry
        Rect symmetric_union = rect2.united(rect1);
        assertRectsIdentical(expected_union, symmetric_union, test_context + " - union symmetry");
    }
    
    void testTransformationPatterns(const Rect& original, const std::string& test_context) {
        // Test translate/translated
        Rect translated = original.translated(5, -3);
        assertRectEquals(translated, original.x() + 5, original.y() - 3, original.width(), original.height(), 
                        test_context + " - translated");
        
        // Test moveTo/movedTo
        Rect moved = original.movedTo(100, 200);
        assertRectEquals(moved, 100, 200, original.width(), original.height(), 
                        test_context + " - moved to");
        
        // Test resize/resized
        Rect resized = original.resized(150, 75);
        assertRectEquals(resized, original.x(), original.y(), 150, 75, 
                        test_context + " - resized");
        
        // Test that original is unchanged for const methods
        assertRectEquals(original, original.x(), original.y(), original.width(), original.height(), 
                        test_context + " - original unchanged");
    }
    
    void testEmptyRectangleEdgeCases(const std::string& test_context) {
        Rect empty = TestRects::empty();
        Rect zeroWidth = TestRects::zeroWidth();
        Rect zeroHeight = TestRects::zeroHeight();
        Rect normal = TestRects::standard();
        
        // Empty rectangles should not contain anything
        testContainmentPatterns(empty, normal, false, test_context + " - empty contains normal");
        testContainmentPatterns(zeroWidth, normal, false, test_context + " - zero width contains normal");
        testContainmentPatterns(zeroHeight, normal, false, test_context + " - zero height contains normal");
        
        // Nothing should contain empty rectangles
        testContainmentPatterns(normal, empty, false, test_context + " - normal contains empty");
        testContainmentPatterns(normal, zeroWidth, false, test_context + " - normal contains zero width");
        testContainmentPatterns(normal, zeroHeight, false, test_context + " - normal contains zero height");
        
        // Empty rectangles should not intersect with anything
        testIntersectionPatterns(empty, normal, false, TestRects::empty(), test_context + " - empty intersects normal");
        testIntersectionPatterns(zeroWidth, normal, false, TestRects::empty(), test_context + " - zero width intersects normal");
        testIntersectionPatterns(zeroHeight, normal, false, TestRects::empty(), test_context + " - zero height intersects normal");
    }
    
    void testOverflowAndBoundaries(const std::string& test_context) {
        // Test maximum values
        Rect maxRect(32767, 32767, 65535, 65535);
        testBasicProperties(maxRect, static_cast<uint32_t>(65535) * 65535, false, true, 
                          test_context + " - maximum values");
        
        // Test minimum coordinates
        Rect minRect(-32768, -32768, 100, 100);
        testBasicProperties(minRect, 10000, false, true, 
                          test_context + " - minimum coordinates");
        
        // Test single pixel
        Rect singlePixel = TestRects::singlePixel();
        testBasicProperties(singlePixel, 1, false, true, 
                          test_context + " - single pixel");
    }

    // ========================================
    // TEST EXECUTION HELPERS
    // ========================================
    
    void executeTestFunction(const std::string& test_name, std::function<void()> test_func) {
        printTestHeader(test_name);
        
        try {
            test_func();
            printTestSuccess(test_name);
        } catch (const TestFramework::AssertionFailure& e) {
            std::cerr << "FAILED: " << test_name << std::endl;
            std::cerr << "  " << e.what() << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "ERROR: " << test_name << std::endl;
            std::cerr << "  Unexpected error: " << e.what() << std::endl;
            throw;
        }
    }
    
    int executeTestSuite(const std::string& suite_name, const std::vector<std::pair<std::string, std::function<void()>>>& tests) {
        std::cout << "Running " << suite_name << "..." << std::endl;
        std::cout << std::string(suite_name.length() + 11, '=') << std::endl;
        
        int passed = 0;
        int total = tests.size();
        
        for (const auto& test : tests) {
            try {
                executeTestFunction(test.first, test.second);
                passed++;
            } catch (...) {
                // Error already printed by executeTestFunction
            }
        }
        
        printTestSuiteSummary(suite_name, total, passed);
        return (passed == total) ? 0 : 1;
    }
    
    void printTestHeader(const std::string& test_name) {
        std::cout << "Testing " << test_name << "... ";
        std::cout.flush();
    }
    
    void printTestSuccess(const std::string& test_name) {
        std::cout << "PASSED" << std::endl;
    }
    
    void printTestSuiteSummary(const std::string& suite_name, int total_tests, int passed_tests) {
        std::cout << std::endl;
        std::cout << suite_name << " Summary:" << std::endl;
        std::cout << "  Total tests: " << total_tests << std::endl;
        std::cout << "  Passed: " << passed_tests << std::endl;
        std::cout << "  Failed: " << (total_tests - passed_tests) << std::endl;
        
        if (passed_tests == total_tests) {
            std::cout << "All " << suite_name << " tests passed!" << std::endl;
        } else {
            std::cout << "Some " << suite_name << " tests failed." << std::endl;
        }
        std::cout << std::endl;
    }

} // namespace RectTestUtilities