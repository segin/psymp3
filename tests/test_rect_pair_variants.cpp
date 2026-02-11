/*
 * test_rect_pair_variants.cpp - Test pair-based constructor and method variants
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <cassert>
#include <iostream>

using namespace PsyMP3::Core;

/**
 * Test pair-based constructors and method variants
 */

void test_pair_constructors() {
    std::cout << "Testing pair-based constructors..." << std::endl;
    
    // Test constructor with position pair and separate dimensions
    Rect rect1({10, 20}, 100, 50);
    assert(rect1.x() == 10);
    assert(rect1.y() == 20);
    assert(rect1.width() == 100);
    assert(rect1.height() == 50);
    std::cout << "  ✓ Constructor with position pair works" << std::endl;
    
    // Test constructor with separate position and size pair
    Rect rect2(30, 40, {80, 60});
    assert(rect2.x() == 30);
    assert(rect2.y() == 40);
    assert(rect2.width() == 80);
    assert(rect2.height() == 60);
    std::cout << "  ✓ Constructor with size pair works" << std::endl;
    
    // Test constructor with both position and size as pairs
    Rect rect3({50, 60}, {120, 70});
    assert(rect3.x() == 50);
    assert(rect3.y() == 60);
    assert(rect3.width() == 120);
    assert(rect3.height() == 70);
    std::cout << "  ✓ Constructor with both pairs works" << std::endl;
}

void test_pair_translate() {
    std::cout << "Testing pair-based translate methods..." << std::endl;
    
    Rect rect(10, 20, 100, 50);
    
    // Test translate with pair (in-place)
    rect.translate({5, -10});
    assert(rect.x() == 15);
    assert(rect.y() == 10);
    assert(rect.width() == 100);
    assert(rect.height() == 50);
    std::cout << "  ✓ translate() with pair works" << std::endl;
    
    // Test translated with pair (const version)
    Rect moved = rect.translated({-5, 10});
    assert(moved.x() == 10);
    assert(moved.y() == 20);
    assert(moved.width() == 100);
    assert(moved.height() == 50);
    assert(rect.x() == 15);  // Original unchanged
    std::cout << "  ✓ translated() with pair works" << std::endl;
}

void test_pair_moveTo() {
    std::cout << "Testing pair-based moveTo methods..." << std::endl;
    
    Rect rect(10, 20, 100, 50);
    
    // Test moveTo with pair (in-place)
    rect.moveTo({100, 200});
    assert(rect.x() == 100);
    assert(rect.y() == 200);
    assert(rect.width() == 100);
    assert(rect.height() == 50);
    std::cout << "  ✓ moveTo() with pair works" << std::endl;
    
    // Test movedTo with pair (const version)
    Rect moved = rect.movedTo({50, 75});
    assert(moved.x() == 50);
    assert(moved.y() == 75);
    assert(moved.width() == 100);
    assert(moved.height() == 50);
    assert(rect.x() == 100);  // Original unchanged
    std::cout << "  ✓ movedTo() with pair works" << std::endl;
}

void test_pair_resize() {
    std::cout << "Testing pair-based resize methods..." << std::endl;
    
    Rect rect(10, 20, 100, 50);
    
    // Test resize with pair (in-place)
    rect.resize({200, 150});
    assert(rect.x() == 10);
    assert(rect.y() == 20);
    assert(rect.width() == 200);
    assert(rect.height() == 150);
    std::cout << "  ✓ resize() with pair works" << std::endl;
    
    // Test resized with pair (const version)
    Rect resized = rect.resized({300, 250});
    assert(resized.x() == 10);
    assert(resized.y() == 20);
    assert(resized.width() == 300);
    assert(resized.height() == 250);
    assert(rect.width() == 200);  // Original unchanged
    std::cout << "  ✓ resized() with pair works" << std::endl;
}

void test_pair_expand_shrink() {
    std::cout << "Testing pair-based expand/shrink methods..." << std::endl;
    
    Rect rect(50, 50, 100, 80);
    
    // Test expand with pair (in-place)
    rect.expand({5, 10});
    assert(rect.x() == 45);
    assert(rect.y() == 40);
    assert(rect.width() == 110);
    assert(rect.height() == 100);
    std::cout << "  ✓ expand() with pair works" << std::endl;
    
    // Test expanded with pair (const version)
    Rect expanded = rect.expanded({5, 5});
    assert(expanded.x() == 40);
    assert(expanded.y() == 35);
    assert(expanded.width() == 120);
    assert(expanded.height() == 110);
    assert(rect.x() == 45);  // Original unchanged
    std::cout << "  ✓ expanded() with pair works" << std::endl;
    
    // Test shrink with pair (in-place)
    rect.shrink({5, 10});
    assert(rect.x() == 50);
    assert(rect.y() == 50);
    assert(rect.width() == 100);
    assert(rect.height() == 80);
    std::cout << "  ✓ shrink() with pair works" << std::endl;
    
    // Test shrunk with pair (const version)
    Rect shrunk = rect.shrunk({10, 10});
    assert(shrunk.x() == 60);
    assert(shrunk.y() == 60);
    assert(shrunk.width() == 80);
    assert(shrunk.height() == 60);
    assert(rect.x() == 50);  // Original unchanged
    std::cout << "  ✓ shrunk() with pair works" << std::endl;
}

void test_pair_adjust() {
    std::cout << "Testing pair-based adjust methods..." << std::endl;
    
    Rect rect(50, 50, 100, 80);
    
    // Test adjust with pairs (in-place)
    rect.adjust({-10, -10}, {20, 20});
    assert(rect.x() == 40);
    assert(rect.y() == 40);
    assert(rect.width() == 120);
    assert(rect.height() == 100);
    std::cout << "  ✓ adjust() with pairs works" << std::endl;
    
    // Test adjusted with pairs (const version)
    Rect adjusted = rect.adjusted({10, 10}, {-20, -20});
    assert(adjusted.x() == 50);
    assert(adjusted.y() == 50);
    assert(adjusted.width() == 100);
    assert(adjusted.height() == 80);
    assert(rect.x() == 40);  // Original unchanged
    std::cout << "  ✓ adjusted() with pairs works" << std::endl;
}

void test_pair_usage_examples() {
    std::cout << "Testing real-world usage examples..." << std::endl;
    
    // Example 1: Creating widgets with structured bindings
    auto pos = std::make_pair<int16_t, int16_t>(100, 200);
    auto size = std::make_pair<uint16_t, uint16_t>(300, 150);
    Rect widget(pos, size);
    assert(widget.x() == 100 && widget.y() == 200);
    assert(widget.width() == 300 && widget.height() == 150);
    std::cout << "  ✓ Widget creation with pairs works" << std::endl;
    
    // Example 2: Animating with offsets
    Rect animRect(50, 50, 100, 100);
    auto offset = std::make_pair<int16_t, int16_t>(5, 5);
    for (int i = 0; i < 10; ++i) {
        animRect.translate(offset);
    }
    assert(animRect.x() == 100);
    assert(animRect.y() == 100);
    std::cout << "  ✓ Animation with pair offsets works" << std::endl;
    
    // Example 3: Padding with pairs
    Rect content(100, 100, 200, 150);
    auto padding = std::make_pair<int16_t, int16_t>(10, 15);
    Rect padded = content.expanded(padding);
    assert(padded.x() == 90);
    assert(padded.y() == 85);
    assert(padded.width() == 220);
    assert(padded.height() == 180);
    std::cout << "  ✓ Padding with pairs works" << std::endl;
}

void test_pos_size_accessors() {
    std::cout << "Testing pos() and size() accessor methods..." << std::endl;
    
    // Test pos() getter
    Rect rect1(10, 20, 100, 50);
    auto position = rect1.pos();
    assert(position.first == 10);
    assert(position.second == 20);
    std::cout << "  ✓ pos() getter works" << std::endl;
    
    // Test pos() setter
    Rect rect2(0, 0, 100, 50);
    rect2.pos({30, 40});
    assert(rect2.x() == 30);
    assert(rect2.y() == 40);
    assert(rect2.width() == 100);
    assert(rect2.height() == 50);
    std::cout << "  ✓ pos() setter works" << std::endl;
    
    // Test size() getter
    Rect rect3(10, 20, 100, 50);
    auto dimensions = rect3.size();
    assert(dimensions.first == 100);
    assert(dimensions.second == 50);
    std::cout << "  ✓ size() getter works" << std::endl;
    
    // Test size() setter
    Rect rect4(10, 20, 0, 0);
    rect4.size({200, 150});
    assert(rect4.x() == 10);
    assert(rect4.y() == 20);
    assert(rect4.width() == 200);
    assert(rect4.height() == 150);
    std::cout << "  ✓ size() setter works" << std::endl;
    
    // Test structured binding with pos()
    Rect rect5(50, 60, 100, 80);
    auto [x, y] = rect5.pos();
    assert(x == 50);
    assert(y == 60);
    std::cout << "  ✓ pos() with structured binding works" << std::endl;
    
    // Test structured binding with size()
    Rect rect6(10, 20, 300, 250);
    auto [w, h] = rect6.size();
    assert(w == 300);
    assert(h == 250);
    std::cout << "  ✓ size() with structured binding works" << std::endl;
}

int main() {
    std::cout << "=== Rect Pair Variants Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        test_pair_constructors();
        std::cout << std::endl;
        
        test_pair_translate();
        std::cout << std::endl;
        
        test_pair_moveTo();
        std::cout << std::endl;
        
        test_pair_resize();
        std::cout << std::endl;
        
        test_pair_expand_shrink();
        std::cout << std::endl;
        
        test_pair_adjust();
        std::cout << std::endl;
        
        test_pair_usage_examples();
        std::cout << std::endl;
        
        test_pos_size_accessors();
        std::cout << std::endl;
        
        std::cout << "=== ALL PAIR VARIANT TESTS PASSED ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
