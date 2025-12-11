/*
 * test_rect_transformation.cpp - Test transformation methods for Rect class
 * This file is part of PsyMP3.
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Transformation Tests
 * @TEST_DESCRIPTION: Tests transformation methods (translate, moveTo, resize, adjust) for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, transformation, translate, resize, adjust
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/core/rect.h"
using PsyMP3::Core::Rect;
#include <iostream>

using namespace TestFramework;
using namespace RectTestUtilities;

void test_translate_methods() {
    // Test translate() method
    Rect rect = TestRects::standard();
    rect.translate(5, -3);
    assertRectEquals(rect, 15, 17, 100, 50, "In-place translate method");
    
    // Test translated() method
    Rect rect2 = TestRects::standard();
    Rect translated = rect2.translated(5, -3);
    assertRectEquals(translated, 15, 17, 100, 50, "Const translated method result");
    // Original should be unchanged
    assertRectEquals(rect2, 10, 20, 100, 50, "Original should be unchanged after translated()");
}

void test_moveTo_methods() {
    // Test moveTo() method
    Rect rect = TestRects::standard();
    rect.moveTo(30, 40);
    assertRectEquals(rect, 30, 40, 100, 50, "In-place moveTo method");
    
    // Test movedTo() method
    Rect rect2 = TestRects::standard();
    Rect moved = rect2.movedTo(30, 40);
    assertRectEquals(moved, 30, 40, 100, 50, "Const movedTo method result");
    // Original should be unchanged
    assertRectEquals(rect2, 10, 20, 100, 50, "Original should be unchanged after movedTo()");
}

void test_resize_methods() {
    // Test resize() method
    Rect rect = TestRects::standard();
    rect.resize(200, 75);
    assertRectEquals(rect, 10, 20, 200, 75, "In-place resize method");
    
    // Test resized() method
    Rect rect2 = TestRects::standard();
    Rect resized = rect2.resized(200, 75);
    assertRectEquals(resized, 10, 20, 200, 75, "Const resized method result");
    // Original should be unchanged
    assertRectEquals(rect2, 10, 20, 100, 50, "Original should be unchanged after resized()");
}

void test_adjust_methods() {
    // Test adjust() method
    Rect rect = TestRects::standard();
    rect.adjust(5, -3, 20, -10);
    assertRectEquals(rect, 15, 17, 120, 40, "In-place adjust method");
    
    // Test adjusted() method
    Rect rect2 = TestRects::standard();
    Rect adjusted = rect2.adjusted(5, -3, 20, -10);
    assertRectEquals(adjusted, 15, 17, 120, 40, "Const adjusted method result");
    // Original should be unchanged
    assertRectEquals(rect2, 10, 20, 100, 50, "Original should be unchanged after adjusted()");
}

void test_overflow_handling() {
    // Test coordinate overflow
    Rect rect(32767, 32767, 100, 50);
    rect.translate(1, 1);
    ASSERT_EQUALS(rect.x(), 32767, "X coordinate should clamp to max");
    ASSERT_EQUALS(rect.y(), 32767, "Y coordinate should clamp to max");
    
    // Test coordinate underflow
    Rect rect2(-32768, -32768, 100, 50);
    rect2.translate(-1, -1);
    ASSERT_EQUALS(rect2.x(), -32768, "X coordinate should clamp to min");
    ASSERT_EQUALS(rect2.y(), -32768, "Y coordinate should clamp to min");
    
    // Test dimension underflow in adjust
    Rect rect3 = TestRects::standard();
    rect3.adjust(0, 0, -200, -100);
    ASSERT_EQUALS(rect3.width(), 0, "Width should clamp to 0");
    ASSERT_EQUALS(rect3.height(), 0, "Height should clamp to 0");
}

int main() {
    // Create test suite
    TestSuite suite("Rectangle Transformation Tests");
    
    // Add test functions
    suite.addTest("Translate Methods", test_translate_methods);
    suite.addTest("MoveTo Methods", test_moveTo_methods);
    suite.addTest("Resize Methods", test_resize_methods);
    suite.addTest("Adjust Methods", test_adjust_methods);
    suite.addTest("Overflow Handling", test_overflow_handling);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}