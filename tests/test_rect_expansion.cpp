#include <iostream>
#include <cassert>
#include "../include/rect.h"

void test_expand_uniform() {
    std::cout << "Testing uniform expansion..." << std::endl;
    
    // Test basic expansion
    Rect r(10, 20, 30, 40);
    r.expand(5);
    
    assert(r.x() == 5);      // moved left by 5
    assert(r.y() == 15);     // moved up by 5
    assert(r.width() == 40); // expanded by 10 (5 on each side)
    assert(r.height() == 50); // expanded by 10 (5 on each side)
    
    std::cout << "Uniform expansion test passed!" << std::endl;
}

void test_expand_directional() {
    std::cout << "Testing directional expansion..." << std::endl;
    
    // Test directional expansion
    Rect r(10, 20, 30, 40);
    r.expand(3, 7);
    
    assert(r.x() == 7);      // moved left by 3
    assert(r.y() == 13);     // moved up by 7
    assert(r.width() == 36); // expanded by 6 (3 on each side)
    assert(r.height() == 54); // expanded by 14 (7 on each side)
    
    std::cout << "Directional expansion test passed!" << std::endl;
}

void test_expanded_const() {
    std::cout << "Testing const expanded methods..." << std::endl;
    
    const Rect r(10, 20, 30, 40);
    Rect expanded = r.expanded(5);
    
    // Original should be unchanged
    assert(r.x() == 10);
    assert(r.y() == 20);
    assert(r.width() == 30);
    assert(r.height() == 40);
    
    // Expanded should be modified
    assert(expanded.x() == 5);
    assert(expanded.y() == 15);
    assert(expanded.width() == 40);
    assert(expanded.height() == 50);
    
    std::cout << "Const expanded methods test passed!" << std::endl;
}

void test_shrink_uniform() {
    std::cout << "Testing uniform shrinking..." << std::endl;
    
    // Test basic shrinking
    Rect r(10, 20, 30, 40);
    r.shrink(5);
    
    assert(r.x() == 15);     // moved right by 5
    assert(r.y() == 25);     // moved down by 5
    assert(r.width() == 20); // shrunk by 10 (5 from each side)
    assert(r.height() == 30); // shrunk by 10 (5 from each side)
    
    std::cout << "Uniform shrinking test passed!" << std::endl;
}

void test_shrink_directional() {
    std::cout << "Testing directional shrinking..." << std::endl;
    
    // Test directional shrinking
    Rect r(10, 20, 30, 40);
    r.shrink(3, 7);
    
    assert(r.x() == 13);     // moved right by 3
    assert(r.y() == 27);     // moved down by 7
    assert(r.width() == 24); // shrunk by 6 (3 from each side)
    assert(r.height() == 26); // shrunk by 14 (7 from each side)
    
    std::cout << "Directional shrinking test passed!" << std::endl;
}

void test_shrunk_const() {
    std::cout << "Testing const shrunk methods..." << std::endl;
    
    const Rect r(10, 20, 30, 40);
    Rect shrunk = r.shrunk(5);
    
    // Original should be unchanged
    assert(r.x() == 10);
    assert(r.y() == 20);
    assert(r.width() == 30);
    assert(r.height() == 40);
    
    // Shrunk should be modified
    assert(shrunk.x() == 15);
    assert(shrunk.y() == 25);
    assert(shrunk.width() == 20);
    assert(shrunk.height() == 30);
    
    std::cout << "Const shrunk methods test passed!" << std::endl;
}

void test_shrink_negative_dimensions() {
    std::cout << "Testing shrinking with negative dimensions..." << std::endl;
    
    // Test shrinking that would create negative dimensions
    Rect r(10, 20, 10, 10);
    r.shrink(10); // This should result in zero dimensions
    
    assert(r.width() == 0);  // Should be clamped to 0
    assert(r.height() == 0); // Should be clamped to 0
    
    std::cout << "Negative dimensions test passed!" << std::endl;
}

void test_expand_shrink_equivalence() {
    std::cout << "Testing expand/shrink equivalence..." << std::endl;
    
    // Test that expanding and then shrinking by the same amount returns to original
    Rect original(10, 20, 30, 40);
    Rect r = original;
    
    r.expand(5);
    r.shrink(5);
    
    assert(r.x() == original.x());
    assert(r.y() == original.y());
    assert(r.width() == original.width());
    assert(r.height() == original.height());
    
    std::cout << "Expand/shrink equivalence test passed!" << std::endl;
}

int main() {
    std::cout << "Running Rect expansion and contraction tests..." << std::endl;
    
    test_expand_uniform();
    test_expand_directional();
    test_expanded_const();
    test_shrink_uniform();
    test_shrink_directional();
    test_shrunk_const();
    test_shrink_negative_dimensions();
    test_expand_shrink_equivalence();
    
    std::cout << "All expansion and contraction tests passed!" << std::endl;
    return 0;
}