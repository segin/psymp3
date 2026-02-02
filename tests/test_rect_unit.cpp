/*
 * test_rect_unit.cpp - Unit tests for Rect class
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * This file contains example-based unit tests for the enhanced Rect class.
 * Unit tests validate specific examples and edge cases to complement the
 * property-based tests in test_rect_properties.cpp.
 *
 * Test Organization:
 * - Tests are organized by functionality (geometric ops, transformations, etc.)
 * - Each test validates specific examples with known expected results
 * - Edge cases (empty rects, overflow, boundaries) are explicitly tested
 * - Tests complement property-based tests with concrete examples
 */

#include "psymp3.h"
#include "test_framework.h"
#include "test_rect_utilities.h"
#include <iostream>
#include <limits>

using PsyMP3::Core::Rect;
using namespace TestFramework;
using namespace RectTestUtilities;

// ========================================
// BASIC CONSTRUCTION AND PROPERTIES
// ========================================

class TestRectConstruction : public TestCase {
public:
    TestRectConstruction() : TestCase("Rect Construction") {}
    
    void runTest() override {
        // Default constructor
        Rect default_rect;
        RECT_ASSERT_EQUALS(default_rect.x(), 0, "Default x should be 0");
        RECT_ASSERT_EQUALS(default_rect.y(), 0, "Default y should be 0");
        RECT_ASSERT_EQUALS(default_rect.width(), 0, "Default width should be 0");
        RECT_ASSERT_EQUALS(default_rect.height(), 0, "Default height should be 0");
        RECT_ASSERT_TRUE(default_rect.isEmpty(), "Default rect should be empty");
        
        // Full constructor
        Rect full_rect(10, 20, 100, 50);
        RECT_ASSERT_EQUALS(full_rect.x(), 10, "Full constructor x");
        RECT_ASSERT_EQUALS(full_rect.y(), 20, "Full constructor y");
        RECT_ASSERT_EQUALS(full_rect.width(), 100, "Full constructor width");
        RECT_ASSERT_EQUALS(full_rect.height(), 50, "Full constructor height");
        RECT_ASSERT_FALSE(full_rect.isEmpty(), "Full rect should not be empty");
        
        // Size-only constructor
        Rect size_rect(80, 60);
        RECT_ASSERT_EQUALS(size_rect.x(), 0, "Size constructor x should be 0");
        RECT_ASSERT_EQUALS(size_rect.y(), 0, "Size constructor y should be 0");
        RECT_ASSERT_EQUALS(size_rect.width(), 80, "Size constructor width");
        RECT_ASSERT_EQUALS(size_rect.height(), 60, "Size constructor height");
    }
};

class TestRectEdgeAccess : public TestCase {
public:
    TestRectEdgeAccess() : TestCase("Rect Edge Access") {}
    
    void runTest() override {
        Rect rect(10, 20, 100, 50);
        
        // Edge coordinates
        RECT_ASSERT_EQUALS(rect.left(), 10, "Left edge");
        RECT_ASSERT_EQUALS(rect.top(), 20, "Top edge");
        RECT_ASSERT_EQUALS(rect.right(), 110, "Right edge");
        RECT_ASSERT_EQUALS(rect.bottom(), 70, "Bottom edge");
        
        // Center coordinates
        RECT_ASSERT_EQUALS(rect.centerX(), 60, "Center X");
        RECT_ASSERT_EQUALS(rect.centerY(), 45, "Center Y");
        
        auto [cx, cy] = rect.center();
        RECT_ASSERT_EQUALS(cx, 60, "Center pair X");
        RECT_ASSERT_EQUALS(cy, 45, "Center pair Y");
    }
};

class TestRectAreaAndEmpty : public TestCase {
public:
    TestRectAreaAndEmpty() : TestCase("Rect Area and Empty") {}
    
    void runTest() override {
        // Normal rectangle
        Rect normal(0, 0, 100, 50);
        RECT_ASSERT_EQUALS(normal.area(), 5000u, "Normal area");
        RECT_ASSERT_FALSE(normal.isEmpty(), "Normal should not be empty");
        
        // Zero width
        Rect zero_width(0, 0, 0, 50);
        RECT_ASSERT_EQUALS(zero_width.area(), 0u, "Zero width area");
        RECT_ASSERT_TRUE(zero_width.isEmpty(), "Zero width should be empty");
        
        // Zero height
        Rect zero_height(0, 0, 100, 0);
        RECT_ASSERT_EQUALS(zero_height.area(), 0u, "Zero height area");
        RECT_ASSERT_TRUE(zero_height.isEmpty(), "Zero height should be empty");
        
        // Single pixel
        Rect single_pixel(5, 5, 1, 1);
        RECT_ASSERT_EQUALS(single_pixel.area(), 1u, "Single pixel area");
        RECT_ASSERT_FALSE(single_pixel.isEmpty(), "Single pixel should not be empty");
        
        // Large rectangle
        Rect large(0, 0, 1000, 2000);
        RECT_ASSERT_EQUALS(large.area(), 2000000u, "Large area");
        RECT_ASSERT_FALSE(large.isEmpty(), "Large should not be empty");
    }
};

// ========================================
// GEOMETRIC OPERATIONS
// ========================================

class TestPointContainment : public TestCase {
public:
    TestPointContainment() : TestCase("Point Containment") {}
    
    void runTest() override {
        Rect rect(10, 20, 100, 50);
        
        // Points inside
        RECT_ASSERT_TRUE(rect.contains(10, 20), "Top-left corner (inclusive)");
        RECT_ASSERT_TRUE(rect.contains(50, 40), "Center point");
        RECT_ASSERT_TRUE(rect.contains(109, 69), "Bottom-right minus 1");
        
        // Points outside
        RECT_ASSERT_FALSE(rect.contains(9, 20), "Left of rect");
        RECT_ASSERT_FALSE(rect.contains(10, 19), "Above rect");
        RECT_ASSERT_FALSE(rect.contains(110, 40), "Right of rect (exclusive)");
        RECT_ASSERT_FALSE(rect.contains(50, 70), "Below rect (exclusive)");
        
        // Empty rectangle never contains points
        Rect empty(0, 0, 0, 0);
        RECT_ASSERT_FALSE(empty.contains(0, 0), "Empty rect at origin");
        RECT_ASSERT_FALSE(empty.contains(10, 10), "Empty rect anywhere");
    }
};

class TestRectangleIntersection : public TestCase {
public:
    TestRectangleIntersection() : TestCase("Rectangle Intersection") {}
    
    void runTest() override {
        Rect rect1(10, 10, 50, 30);
        
        // Overlapping rectangles
        Rect rect2(30, 20, 40, 25);
        RECT_ASSERT_TRUE(rect1.intersects(rect2), "Overlapping rects");
        Rect intersection = rect1.intersection(rect2);
        assertRectEquals(intersection, 30, 20, 30, 20, "Intersection result");
        
        // Non-overlapping rectangles
        Rect rect3(100, 100, 20, 20);
        RECT_ASSERT_FALSE(rect1.intersects(rect3), "Non-overlapping rects");
        Rect no_intersection = rect1.intersection(rect3);
        RECT_ASSERT_TRUE(no_intersection.isEmpty(), "No intersection should be empty");
        
        // Touching edges (exclusive bounds - should NOT intersect)
        Rect rect4(60, 10, 20, 20);
        RECT_ASSERT_FALSE(rect1.intersects(rect4), "Touching edges (exclusive bounds)");
        
        // Contained rectangle
        Rect rect5(20, 15, 10, 10);
        RECT_ASSERT_TRUE(rect1.intersects(rect5), "Contained rect");
        Rect contained_intersection = rect1.intersection(rect5);
        assertRectsIdentical(rect5, contained_intersection, "Contained intersection");
        
        // Empty rectangle
        Rect empty(0, 0, 0, 0);
        RECT_ASSERT_FALSE(rect1.intersects(empty), "Empty rect intersection");
    }
};

class TestRectangleUnion : public TestCase {
public:
    TestRectangleUnion() : TestCase("Rectangle Union") {}
    
    void runTest() override {
        // Adjacent rectangles
        Rect rect1(10, 10, 50, 30);
        Rect rect2(60, 10, 40, 30);
        Rect union_result = rect1.united(rect2);
        assertRectEquals(union_result, 10, 10, 90, 30, "Adjacent union");
        
        // Overlapping rectangles
        Rect rect3(30, 20, 40, 25);
        Rect overlap_union = rect1.united(rect3);
        assertRectEquals(overlap_union, 10, 10, 60, 35, "Overlapping union");
        
        // Separated rectangles
        Rect rect4(100, 100, 20, 20);
        Rect separated_union = rect1.united(rect4);
        assertRectEquals(separated_union, 10, 10, 110, 110, "Separated union");
        
        // Union with empty rectangle
        Rect empty(0, 0, 0, 0);
        Rect empty_union = rect1.united(empty);
        assertRectsIdentical(rect1, empty_union, "Union with empty");
        
        // Symmetry test
        Rect sym1 = rect1.united(rect2);
        Rect sym2 = rect2.united(rect1);
        assertRectsIdentical(sym1, sym2, "Union symmetry");
    }
};

// ========================================
// TRANSFORMATION OPERATIONS
// ========================================

class TestTranslation : public TestCase {
public:
    TestTranslation() : TestCase("Translation") {}
    
    void runTest() override {
        Rect rect(10, 20, 100, 50);
        
        // Translate right and down
        Rect translated1 = rect.translated(5, 10);
        assertRectEquals(translated1, 15, 30, 100, 50, "Translate right/down");
        
        // Translate left and up
        Rect translated2 = rect.translated(-5, -10);
        assertRectEquals(translated2, 5, 10, 100, 50, "Translate left/up");
        
        // Zero translation
        Rect translated3 = rect.translated(0, 0);
        assertRectsIdentical(rect, translated3, "Zero translation");
        
        // In-place translation
        Rect rect_copy = rect;
        rect_copy.translate(5, 10);
        assertRectEquals(rect_copy, 15, 30, 100, 50, "In-place translate");
        
        // Original unchanged
        assertRectEquals(rect, 10, 20, 100, 50, "Original unchanged");
    }
};

class TestMoveTo : public TestCase {
public:
    TestMoveTo() : TestCase("MoveTo") {}
    
    void runTest() override {
        Rect rect(10, 20, 100, 50);
        
        // Move to new position
        Rect moved = rect.movedTo(50, 60);
        assertRectEquals(moved, 50, 60, 100, 50, "Moved to new position");
        
        // Move to origin
        Rect moved_origin = rect.movedTo(0, 0);
        assertRectEquals(moved_origin, 0, 0, 100, 50, "Moved to origin");
        
        // Move to negative coordinates
        Rect moved_negative = rect.movedTo(-10, -20);
        assertRectEquals(moved_negative, -10, -20, 100, 50, "Moved to negative");
        
        // In-place moveTo
        Rect rect_copy = rect;
        rect_copy.moveTo(50, 60);
        assertRectEquals(rect_copy, 50, 60, 100, 50, "In-place moveTo");
    }
};

class TestResize : public TestCase {
public:
    TestResize() : TestCase("Resize") {}
    
    void runTest() override {
        Rect rect(10, 20, 100, 50);
        
        // Resize larger
        Rect resized_larger = rect.resized(150, 75);
        assertRectEquals(resized_larger, 10, 20, 150, 75, "Resized larger");
        
        // Resize smaller
        Rect resized_smaller = rect.resized(50, 25);
        assertRectEquals(resized_smaller, 10, 20, 50, 25, "Resized smaller");
        
        // Resize to zero
        Rect resized_zero = rect.resized(0, 0);
        assertRectEquals(resized_zero, 10, 20, 0, 0, "Resized to zero");
        RECT_ASSERT_TRUE(resized_zero.isEmpty(), "Resized to zero should be empty");
        
        // In-place resize
        Rect rect_copy = rect;
        rect_copy.resize(150, 75);
        assertRectEquals(rect_copy, 10, 20, 150, 75, "In-place resize");
    }
};

class TestExpansionShrinking : public TestCase {
public:
    TestExpansionShrinking() : TestCase("Expansion and Shrinking") {}
    
    void runTest() override {
        Rect rect(50, 50, 100, 60);
        
        // Uniform expansion
        Rect expanded = rect.expanded(10);
        assertRectEquals(expanded, 40, 40, 120, 80, "Uniform expansion");
        
        // Directional expansion
        Rect expanded_dir = rect.expanded(5, 10);
        assertRectEquals(expanded_dir, 45, 40, 110, 80, "Directional expansion");
        
        // Uniform shrinking
        Rect shrunk = rect.shrunk(10);
        assertRectEquals(shrunk, 60, 60, 80, 40, "Uniform shrinking");
        
        // Directional shrinking
        Rect shrunk_dir = rect.shrunk(5, 10);
        assertRectEquals(shrunk_dir, 55, 60, 90, 40, "Directional shrinking");
        
        // Shrink to empty
        Rect shrunk_empty = rect.shrunk(50);
        RECT_ASSERT_TRUE(shrunk_empty.isEmpty(), "Shrunk to empty");
    }
};

class TestCentering : public TestCase {
public:
    TestCentering() : TestCase("Centering") {}
    
    void runTest() override {
        // Center small rect in large container
        Rect small(0, 0, 50, 30);
        Rect container(0, 0, 200, 100);
        Rect centered = small.centeredIn(container);
        
        // Check centered position (allowing ±1 for integer division)
        int16_t expected_x = 75;  // (200 - 50) / 2
        int16_t expected_y = 35;  // (100 - 30) / 2
        RECT_ASSERT_EQUALS(centered.x(), expected_x, "Centered X");
        RECT_ASSERT_EQUALS(centered.y(), expected_y, "Centered Y");
        RECT_ASSERT_EQUALS(centered.width(), 50, "Centered width unchanged");
        RECT_ASSERT_EQUALS(centered.height(), 30, "Centered height unchanged");
        
        // Center at origin
        Rect origin_container(0, 0, 100, 100);
        Rect origin_centered = small.centeredIn(origin_container);
        RECT_ASSERT_EQUALS(origin_centered.x(), 25, "Origin centered X");
        RECT_ASSERT_EQUALS(origin_centered.y(), 35, "Origin centered Y");
        
        // Center in offset container
        Rect offset_container(50, 50, 100, 100);
        Rect offset_centered = small.centeredIn(offset_container);
        RECT_ASSERT_EQUALS(offset_centered.x(), 75, "Offset centered X");
        RECT_ASSERT_EQUALS(offset_centered.y(), 85, "Offset centered Y");
    }
};

// ========================================
// MODERN C++ FEATURES
// ========================================

class TestEqualityOperators : public TestCase {
public:
    TestEqualityOperators() : TestCase("Equality Operators") {}
    
    void runTest() override {
        Rect rect1(10, 20, 100, 50);
        Rect rect2(10, 20, 100, 50);
        Rect rect3(10, 20, 100, 51);
        
        // Equality
        RECT_ASSERT_TRUE(rect1 == rect2, "Equal rects");
        RECT_ASSERT_FALSE(rect1 == rect3, "Unequal rects");
        
        // Inequality
        RECT_ASSERT_FALSE(rect1 != rect2, "Equal rects inequality");
        RECT_ASSERT_TRUE(rect1 != rect3, "Unequal rects inequality");
        
        // Reflexivity
        RECT_ASSERT_TRUE(rect1 == rect1, "Reflexivity");
        
        // Symmetry
        RECT_ASSERT_TRUE(rect2 == rect1, "Symmetry");
    }
};

class TestStringRepresentation : public TestCase {
public:
    TestStringRepresentation() : TestCase("String Representation") {}
    
    void runTest() override {
        Rect rect(10, 20, 100, 50);
        std::string str = rect.toString();
        
        // Should contain all values
        RECT_ASSERT_TRUE(str.find("10") != std::string::npos, "Contains x");
        RECT_ASSERT_TRUE(str.find("20") != std::string::npos, "Contains y");
        RECT_ASSERT_TRUE(str.find("100") != std::string::npos, "Contains width");
        RECT_ASSERT_TRUE(str.find("50") != std::string::npos, "Contains height");
        
        // Negative coordinates
        Rect negative(-10, -20, 30, 40);
        std::string neg_str = negative.toString();
        RECT_ASSERT_TRUE(neg_str.find("-10") != std::string::npos, "Contains negative x");
        RECT_ASSERT_TRUE(neg_str.find("-20") != std::string::npos, "Contains negative y");
    }
};

// ========================================
// EDGE CASES AND VALIDATION
// ========================================

class TestEmptyRectangles : public TestCase {
public:
    TestEmptyRectangles() : TestCase("Empty Rectangles") {}
    
    void runTest() override {
        Rect empty(0, 0, 0, 0);
        Rect zero_width(10, 10, 0, 20);
        Rect zero_height(10, 10, 20, 0);
        Rect normal(10, 10, 20, 20);
        
        // Empty rectangles don't contain points
        RECT_ASSERT_FALSE(empty.contains(0, 0), "Empty contains origin");
        RECT_ASSERT_FALSE(zero_width.contains(10, 15), "Zero width contains");
        RECT_ASSERT_FALSE(zero_height.contains(15, 10), "Zero height contains");
        
        // Empty rectangles don't intersect
        RECT_ASSERT_FALSE(empty.intersects(normal), "Empty intersects normal");
        RECT_ASSERT_FALSE(zero_width.intersects(normal), "Zero width intersects");
        RECT_ASSERT_FALSE(zero_height.intersects(normal), "Zero height intersects");
        
        // Union with empty
        Rect union_empty = normal.united(empty);
        assertRectsIdentical(normal, union_empty, "Union with empty");
    }
};

class TestNegativeCoordinates : public TestCase {
public:
    TestNegativeCoordinates() : TestCase("Negative Coordinates") {}
    
    void runTest() override {
        Rect negative(-10, -20, 30, 40);
        
        // Basic properties
        RECT_ASSERT_EQUALS(negative.x(), -10, "Negative x");
        RECT_ASSERT_EQUALS(negative.y(), -20, "Negative y");
        RECT_ASSERT_EQUALS(negative.right(), 20, "Right with negative x");
        RECT_ASSERT_EQUALS(negative.bottom(), 20, "Bottom with negative y");
        
        // Containment with negative coords
        RECT_ASSERT_TRUE(negative.contains(-10, -20), "Contains negative corner");
        RECT_ASSERT_TRUE(negative.contains(0, 0), "Contains origin");
        RECT_ASSERT_FALSE(negative.contains(-11, -20), "Outside negative");
        
        // Operations with negative coords
        Rect translated = negative.translated(-5, -5);
        assertRectEquals(translated, -15, -25, 30, 40, "Translate negative");
    }
};

class TestBoundaryConditions : public TestCase {
public:
    TestBoundaryConditions() : TestCase("Boundary Conditions") {}
    
    void runTest() override {
        // Maximum positive coordinates
        Rect max_pos(32767, 32767, 100, 100);
        RECT_ASSERT_EQUALS(max_pos.x(), 32767, "Max positive x");
        RECT_ASSERT_EQUALS(max_pos.y(), 32767, "Max positive y");
        
        // Minimum negative coordinates
        Rect max_neg(-32768, -32768, 100, 100);
        RECT_ASSERT_EQUALS(max_neg.x(), -32768, "Max negative x");
        RECT_ASSERT_EQUALS(max_neg.y(), -32768, "Max negative y");
        
        // Maximum dimensions
        Rect max_dim(0, 0, 65535, 65535);
        RECT_ASSERT_EQUALS(max_dim.width(), 65535, "Max width");
        RECT_ASSERT_EQUALS(max_dim.height(), 65535, "Max height");
        
        // Area calculation with max dimensions
        uint32_t expected_area = static_cast<uint32_t>(65535) * 65535;
        RECT_ASSERT_EQUALS(max_dim.area(), expected_area, "Max area");
    }
};

// ========================================
// MAIN TEST RUNNER
// ========================================

int main() {
    TestSuite suite("Rect Unit Tests");
    
    // Basic construction and properties
    suite.addTest(std::make_unique<TestRectConstruction>());
    suite.addTest(std::make_unique<TestRectEdgeAccess>());
    suite.addTest(std::make_unique<TestRectAreaAndEmpty>());
    
    // Geometric operations
    suite.addTest(std::make_unique<TestPointContainment>());
    suite.addTest(std::make_unique<TestRectangleIntersection>());
    suite.addTest(std::make_unique<TestRectangleUnion>());
    
    // Transformation operations
    suite.addTest(std::make_unique<TestTranslation>());
    suite.addTest(std::make_unique<TestMoveTo>());
    suite.addTest(std::make_unique<TestResize>());
    suite.addTest(std::make_unique<TestExpansionShrinking>());
    suite.addTest(std::make_unique<TestCentering>());
    
    // Modern C++ features
    suite.addTest(std::make_unique<TestEqualityOperators>());
    suite.addTest(std::make_unique<TestStringRepresentation>());
    
    // Edge cases and validation
    suite.addTest(std::make_unique<TestEmptyRectangles>());
    suite.addTest(std::make_unique<TestNegativeCoordinates>());
    suite.addTest(std::make_unique<TestBoundaryConditions>());
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print summary
    suite.printResults(results);
    
    // Return exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
