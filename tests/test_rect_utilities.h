/*
 * test_rect_utilities.h - Common utilities for Rect testing
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef TEST_RECT_UTILITIES_H
#define TEST_RECT_UTILITIES_H

#include "test_framework.h"
#include "../include/rect.h"
#include <string>
#include <vector>

namespace RectTestUtilities {

    // ========================================
    // COMMON TEST PATTERNS FOR RECT
    // ========================================
    
    /**
     * @brief Test metadata structure for Rect tests
     */
    struct RectTestMetadata {
        std::string name;
        std::string description;
        std::vector<std::string> requirements;
        std::string author;
        std::string copyright;
        
        RectTestMetadata(const std::string& test_name, const std::string& test_desc)
            : name(test_name), description(test_desc), author("Kirn Gill <segin2005@gmail.com>"), 
              copyright("Copyright © 2025 Kirn Gill <segin2005@gmail.com>") {}
    };

    // ========================================
    // COMMON RECT TEST SCENARIOS
    // ========================================
    
    /**
     * @brief Create test rectangles for common scenarios
     */
    namespace TestRects {
        
        /**
         * @brief Create a standard test rectangle (10, 20, 100, 50)
         */
        Rect standard();
        
        /**
         * @brief Create an empty rectangle (0, 0, 0, 0)
         */
        Rect empty();
        
        /**
         * @brief Create a zero-width rectangle (10, 10, 0, 20)
         */
        Rect zeroWidth();
        
        /**
         * @brief Create a zero-height rectangle (10, 10, 20, 0)
         */
        Rect zeroHeight();
        
        /**
         * @brief Create a single pixel rectangle (5, 5, 1, 1)
         */
        Rect singlePixel();
        
        /**
         * @brief Create a rectangle at origin (0, 0, 50, 50)
         */
        Rect atOrigin();
        
        /**
         * @brief Create a rectangle with negative coordinates (-10, -10, 20, 20)
         */
        Rect withNegativeCoords();
        
        /**
         * @brief Create a large rectangle (0, 0, 65535, 65535)
         */
        Rect large();
        
        /**
         * @brief Create a container rectangle for centering tests (0, 0, 100, 100)
         */
        Rect container();
        
        /**
         * @brief Create an offset container rectangle (20, 30, 60, 40)
         */
        Rect offsetContainer();
    }

    // ========================================
    // ENHANCED ASSERTION UTILITIES
    // ========================================
    
    /**
     * @brief Assert rectangle has specific coordinates and dimensions
     */
    void assertRectEquals(const Rect& rect, int16_t x, int16_t y, uint16_t width, uint16_t height, const std::string& message);
    
    /**
     * @brief Assert two rectangles are identical
     */
    void assertRectsIdentical(const Rect& expected, const Rect& actual, const std::string& message);
    
    /**
     * @brief Assert rectangle is empty (width or height is 0)
     */
    void assertRectEmpty(const Rect& rect, const std::string& message);
    
    /**
     * @brief Assert rectangle is not empty
     */
    void assertRectNotEmpty(const Rect& rect, const std::string& message);
    
    /**
     * @brief Assert rectangle is valid (width > 0 and height > 0)
     */
    void assertRectValid(const Rect& rect, const std::string& message);
    
    /**
     * @brief Assert rectangle is invalid (width == 0 or height == 0)
     */
    void assertRectInvalid(const Rect& rect, const std::string& message);
    
    /**
     * @brief Assert rectangle area equals expected value
     */
    void assertRectArea(const Rect& rect, uint32_t expected_area, const std::string& message);
    
    /**
     * @brief Assert rectangle center point
     */
    void assertRectCenter(const Rect& rect, int16_t expected_center_x, int16_t expected_center_y, const std::string& message);

    // ========================================
    // COMMON TEST PATTERNS
    // ========================================
    
    /**
     * @brief Test basic rectangle properties (area, isEmpty, isValid)
     */
    void testBasicProperties(const Rect& rect, uint32_t expected_area, bool should_be_empty, bool should_be_valid, const std::string& test_context);
    
    /**
     * @brief Test rectangle containment patterns
     */
    void testContainmentPatterns(const Rect& container, const Rect& inner, bool should_contain, const std::string& test_context);
    
    /**
     * @brief Test rectangle intersection patterns
     */
    void testIntersectionPatterns(const Rect& rect1, const Rect& rect2, bool should_intersect, const Rect& expected_intersection, const std::string& test_context);
    
    /**
     * @brief Test rectangle union patterns
     */
    void testUnionPatterns(const Rect& rect1, const Rect& rect2, const Rect& expected_union, const std::string& test_context);
    
    /**
     * @brief Test rectangle transformation patterns
     */
    void testTransformationPatterns(const Rect& original, const std::string& test_context);
    
    /**
     * @brief Test edge cases for empty rectangles
     */
    void testEmptyRectangleEdgeCases(const std::string& test_context);
    
    /**
     * @brief Test overflow and boundary conditions
     */
    void testOverflowAndBoundaries(const std::string& test_context);

    // ========================================
    // TEST EXECUTION HELPERS
    // ========================================
    
    /**
     * @brief Execute a test function with standard error handling and reporting
     */
    void executeTestFunction(const std::string& test_name, std::function<void()> test_func);
    
    /**
     * @brief Execute multiple test functions as a test suite
     */
    int executeTestSuite(const std::string& suite_name, const std::vector<std::pair<std::string, std::function<void()>>>& tests);
    
    /**
     * @brief Print test header with consistent formatting
     */
    void printTestHeader(const std::string& test_name);
    
    /**
     * @brief Print test success message
     */
    void printTestSuccess(const std::string& test_name);
    
    /**
     * @brief Print test suite summary
     */
    void printTestSuiteSummary(const std::string& suite_name, int total_tests, int passed_tests);

    // ========================================
    // COMPATIBILITY LAYER
    // ========================================
    
    /**
     * @brief Compatibility macros for existing assert() usage
     * These allow gradual migration from assert() to TestFramework assertions
     */
    #define RECT_ASSERT_TRUE(condition, message) \
        do { \
            if (!(condition)) { \
                throw TestFramework::AssertionFailure(std::string("RECT_ASSERT_TRUE failed: ") + (message)); \
            } \
        } while(0)
    
    #define RECT_ASSERT_FALSE(condition, message) \
        do { \
            if ((condition)) { \
                throw TestFramework::AssertionFailure(std::string("RECT_ASSERT_FALSE failed: ") + (message)); \
            } \
        } while(0)
    
    #define RECT_ASSERT_EQUALS(expected, actual, message) \
        do { \
            if (!((expected) == (actual))) { \
                std::ostringstream oss; \
                oss << "RECT_ASSERT_EQUALS failed: " << (message) \
                    << " - Expected: " << (expected) << ", Got: " << (actual); \
                throw TestFramework::AssertionFailure(oss.str()); \
            } \
        } while(0)

} // namespace RectTestUtilities

#endif // TEST_RECT_UTILITIES_H