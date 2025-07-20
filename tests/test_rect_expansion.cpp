/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Expansion Tests
 * @TEST_DESCRIPTION: Tests expansion and contraction methods for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, expansion, contraction, transformation
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/rect.h"
#include <iostream>

using namespace TestFramework;
using namespace RectTestUtilities;

void test_expand_uniform() {
    // Test basic expansion
    Rect r(10, 20, 30, 40);
    r.expand(5);
    
    assertRectEquals(r, 5, 15, 40, 50, "Uniform expansion by 5");
}

void test_expand_directional() {
    // Test directional expansion
    Rect r(10, 20, 30, 40);
    r.expand(3, 7);
    
    assertRectEquals(r, 7, 13, 36, 54, "Directional expansion by 3,7");
}

void test_expanded_const() {
    const Rect r(10, 20, 30, 40);
    Rect expanded = r.expanded(5);
    
    // Original should be unchanged
    assertRectEquals(r, 10, 20, 30, 40, "Original rectangle should be unchanged");
    
    // Expanded should be modified
    assertRectEquals(expanded, 5, 15, 40, 50, "Expanded rectangle should be modified");
}

void test_shrink_uniform() {
    // Test basic shrinking
    Rect r(10, 20, 30, 40);
    r.shrink(5);
    
    assertRectEquals(r, 15, 25, 20, 30, "Uniform shrinking by 5");
}

void test_shrink_directional() {
    // Test directional shrinking
    Rect r(10, 20, 30, 40);
    r.shrink(3, 7);
    
    assertRectEquals(r, 13, 27, 24, 26, "Directional shrinking by 3,7");
}

void test_shrunk_const() {
    const Rect r(10, 20, 30, 40);
    Rect shrunk = r.shrunk(5);
    
    // Original should be unchanged
    assertRectEquals(r, 10, 20, 30, 40, "Original rectangle should be unchanged");
    
    // Shrunk should be modified
    assertRectEquals(shrunk, 15, 25, 20, 30, "Shrunk rectangle should be modified");
}

void test_shrink_negative_dimensions() {
    // Test shrinking that would create negative dimensions
    Rect r(10, 20, 10, 10);
    r.shrink(10); // This should result in zero dimensions
    
    ASSERT_EQUALS(r.width(), 0, "Width should be clamped to 0");
    ASSERT_EQUALS(r.height(), 0, "Height should be clamped to 0");
}

void test_expand_shrink_equivalence() {
    // Test that expanding and then shrinking by the same amount returns to original
    Rect original = TestRects::standard();
    Rect r = original;
    
    r.expand(5);
    r.shrink(5);
    
    assertRectsIdentical(r, original, "Expand then shrink should return to original");
}

int main() {
    // Create test suite
    TestSuite suite("Rectangle Expansion and Contraction Tests");
    
    // Add test functions
    suite.addTest("Uniform Expansion", test_expand_uniform);
    suite.addTest("Directional Expansion", test_expand_directional);
    suite.addTest("Const Expanded Methods", test_expanded_const);
    suite.addTest("Uniform Shrinking", test_shrink_uniform);
    suite.addTest("Directional Shrinking", test_shrink_directional);
    suite.addTest("Const Shrunk Methods", test_shrunk_const);
    suite.addTest("Shrink with Negative Dimensions", test_shrink_negative_dimensions);
    suite.addTest("Expand/Shrink Equivalence", test_expand_shrink_equivalence);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}