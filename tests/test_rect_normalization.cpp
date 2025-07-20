#include "rect.h"
#include <iostream>
#include <cassert>

void testNormalization() {
    std::cout << "Testing rectangle normalization..." << std::endl;
    
    // Test with positive dimensions (should remain unchanged)
    Rect r1(10, 20, 30, 40);
    Rect normalized1 = r1.normalized();
    assert(normalized1.x() == 10);
    assert(normalized1.y() == 20);
    assert(normalized1.width() == 30);
    assert(normalized1.height() == 40);
    std::cout << "Positive dimensions test passed!" << std::endl;
    
    // Test in-place normalization with positive dimensions
    Rect r2(5, 15, 25, 35);
    r2.normalize();
    assert(r2.x() == 5);
    assert(r2.y() == 15);
    assert(r2.width() == 25);
    assert(r2.height() == 35);
    std::cout << "In-place normalization with positive dimensions test passed!" << std::endl;
}

void testSafeArithmetic() {
    std::cout << "Testing safe arithmetic with edge cases..." << std::endl;
    
    // Test near maximum coordinates
    Rect r1(32767, 32767, 1, 1);
    assert(r1.right() == 32767);  // Should be clamped
    assert(r1.bottom() == 32767); // Should be clamped
    std::cout << "Maximum coordinate edge case test passed!" << std::endl;
    
    // Test center calculation with large coordinates
    Rect r2(32000, 32000, 1000, 1000);
    int16_t centerX = r2.centerX();
    int16_t centerY = r2.centerY();
    // Should not overflow
    assert(centerX >= 32000);
    assert(centerY >= 32000);
    std::cout << "Center calculation with large coordinates test passed!" << std::endl;
    
    // Test with zero dimensions
    Rect r3(100, 200, 0, 0);
    assert(r3.right() == 100);
    assert(r3.bottom() == 200);
    assert(r3.centerX() == 100);
    assert(r3.centerY() == 200);
    std::cout << "Zero dimensions test passed!" << std::endl;
}

void testCoordinateSystemLimits() {
    std::cout << "Testing coordinate system limits..." << std::endl;
    
    // Test minimum coordinates
    Rect r1(-32768, -32768, 1, 1);
    assert(r1.x() == -32768);
    assert(r1.y() == -32768);
    assert(r1.right() == -32767);
    assert(r1.bottom() == -32767);
    std::cout << "Minimum coordinate test passed!" << std::endl;
    
    // Test maximum dimensions
    Rect r2(0, 0, 65535, 65535);
    assert(r2.width() == 65535);
    assert(r2.height() == 65535);
    assert(r2.area() == static_cast<uint32_t>(65535) * 65535);
    std::cout << "Maximum dimensions test passed!" << std::endl;
}

int main() {
    std::cout << "Running Rect normalization and coordinate system tests..." << std::endl;
    
    testNormalization();
    testSafeArithmetic();
    testCoordinateSystemLimits();
    
    std::cout << "All normalization and coordinate system tests passed!" << std::endl;
    return 0;
}