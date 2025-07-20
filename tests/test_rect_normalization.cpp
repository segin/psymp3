/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Normalization Tests
 * @TEST_DESCRIPTION: Tests normalization and coordinate system limits for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, normalization, limits, coordinates
 * @TEST_METADATA_END
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/rect.h"
#include <iostream>

using namespace TestFramework;
using namespace RectTestUtilities;

void testNormalization() {
    // Test with positive dimensions (should remain unchanged)
    Rect r1(10, 20, 30, 40);
    Rect normalized1 = r1.normalized();
    assertRectEquals(normalized1, 10, 20, 30, 40, "Normalized positive dimensions should remain unchanged");
    
    // Test in-place normalization with positive dimensions
    Rect r2(5, 15, 25, 35);
    r2.normalize();
    assertRectEquals(r2, 5, 15, 25, 35, "In-place normalization with positive dimensions");
}

void testSafeArithmetic() {
    // Test near maximum coordinates
    Rect r1(32767, 32767, 1, 1);
    ASSERT_EQUALS(r1.right(), 32767, "Right edge should be clamped at maximum");
    ASSERT_EQUALS(r1.bottom(), 32767, "Bottom edge should be clamped at maximum");
    
    // Test center calculation with large coordinates
    Rect r2(32000, 32000, 1000, 1000);
    int16_t centerX = r2.centerX();
    int16_t centerY = r2.centerY();
    // Should not overflow
    ASSERT_TRUE(centerX >= 32000, "Center X should not overflow");
    ASSERT_TRUE(centerY >= 32000, "Center Y should not overflow");
    
    // Test with zero dimensions
    Rect r3(100, 200, 0, 0);
    ASSERT_EQUALS(r3.right(), 100, "Right edge of zero width rectangle");
    ASSERT_EQUALS(r3.bottom(), 200, "Bottom edge of zero height rectangle");
    ASSERT_EQUALS(r3.centerX(), 100, "Center X of zero width rectangle");
    ASSERT_EQUALS(r3.centerY(), 200, "Center Y of zero height rectangle");
}

void testCoordinateSystemLimits() {
    // Test minimum coordinates
    Rect r1(-32768, -32768, 1, 1);
    assertRectEquals(r1, -32768, -32768, 1, 1, "Minimum coordinate rectangle");
    ASSERT_EQUALS(r1.right(), -32767, "Right edge at minimum coordinate");
    ASSERT_EQUALS(r1.bottom(), -32767, "Bottom edge at minimum coordinate");
    
    // Test maximum dimensions
    Rect r2 = TestRects::large();
    ASSERT_EQUALS(r2.width(), 65535, "Maximum width");
    ASSERT_EQUALS(r2.height(), 65535, "Maximum height");
    assertRectArea(r2, static_cast<uint32_t>(65535) * 65535, "Maximum area calculation");
}

int main() {
    // Create test suite
    TestSuite suite("Rectangle Normalization and Coordinate System Tests");
    
    // Add test functions
    suite.addTest("Normalization", testNormalization);
    suite.addTest("Safe Arithmetic", testSafeArithmetic);
    suite.addTest("Coordinate System Limits", testCoordinateSystemLimits);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}