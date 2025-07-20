/*
 * test_rect_transformation.cpp - Test transformation methods for Rect class
 * This file is part of PsyMP3.
 */

#include "../include/psymp3.h"
#include <iostream>
#include <cassert>

void test_translate_methods() {
    std::cout << "Testing translate methods..." << std::endl;
    
    // Test translate() method
    Rect rect(10, 20, 100, 50);
    rect.translate(5, -3);
    assert(rect.x() == 15);
    assert(rect.y() == 17);
    assert(rect.width() == 100);
    assert(rect.height() == 50);
    
    // Test translated() method
    Rect rect2(10, 20, 100, 50);
    Rect translated = rect2.translated(5, -3);
    assert(translated.x() == 15);
    assert(translated.y() == 17);
    assert(translated.width() == 100);
    assert(translated.height() == 50);
    // Original should be unchanged
    assert(rect2.x() == 10);
    assert(rect2.y() == 20);
    
    std::cout << "translate methods: PASSED" << std::endl;
}

void test_moveTo_methods() {
    std::cout << "Testing moveTo methods..." << std::endl;
    
    // Test moveTo() method
    Rect rect(10, 20, 100, 50);
    rect.moveTo(30, 40);
    assert(rect.x() == 30);
    assert(rect.y() == 40);
    assert(rect.width() == 100);
    assert(rect.height() == 50);
    
    // Test movedTo() method
    Rect rect2(10, 20, 100, 50);
    Rect moved = rect2.movedTo(30, 40);
    assert(moved.x() == 30);
    assert(moved.y() == 40);
    assert(moved.width() == 100);
    assert(moved.height() == 50);
    // Original should be unchanged
    assert(rect2.x() == 10);
    assert(rect2.y() == 20);
    
    std::cout << "moveTo methods: PASSED" << std::endl;
}

void test_resize_methods() {
    std::cout << "Testing resize methods..." << std::endl;
    
    // Test resize() method
    Rect rect(10, 20, 100, 50);
    rect.resize(200, 75);
    assert(rect.x() == 10);
    assert(rect.y() == 20);
    assert(rect.width() == 200);
    assert(rect.height() == 75);
    
    // Test resized() method
    Rect rect2(10, 20, 100, 50);
    Rect resized = rect2.resized(200, 75);
    assert(resized.x() == 10);
    assert(resized.y() == 20);
    assert(resized.width() == 200);
    assert(resized.height() == 75);
    // Original should be unchanged
    assert(rect2.width() == 100);
    assert(rect2.height() == 50);
    
    std::cout << "resize methods: PASSED" << std::endl;
}

void test_adjust_methods() {
    std::cout << "Testing adjust methods..." << std::endl;
    
    // Test adjust() method
    Rect rect(10, 20, 100, 50);
    rect.adjust(5, -3, 20, -10);
    assert(rect.x() == 15);
    assert(rect.y() == 17);
    assert(rect.width() == 120);
    assert(rect.height() == 40);
    
    // Test adjusted() method
    Rect rect2(10, 20, 100, 50);
    Rect adjusted = rect2.adjusted(5, -3, 20, -10);
    assert(adjusted.x() == 15);
    assert(adjusted.y() == 17);
    assert(adjusted.width() == 120);
    assert(adjusted.height() == 40);
    // Original should be unchanged
    assert(rect2.x() == 10);
    assert(rect2.y() == 20);
    assert(rect2.width() == 100);
    assert(rect2.height() == 50);
    
    std::cout << "adjust methods: PASSED" << std::endl;
}

void test_overflow_handling() {
    std::cout << "Testing overflow handling..." << std::endl;
    
    // Test coordinate overflow
    Rect rect(32767, 32767, 100, 50);
    rect.translate(1, 1);
    assert(rect.x() == 32767); // Should clamp to max
    assert(rect.y() == 32767); // Should clamp to max
    
    // Test coordinate underflow
    Rect rect2(-32768, -32768, 100, 50);
    rect2.translate(-1, -1);
    assert(rect2.x() == -32768); // Should clamp to min
    assert(rect2.y() == -32768); // Should clamp to min
    
    // Test dimension underflow in adjust
    Rect rect3(10, 20, 100, 50);
    rect3.adjust(0, 0, -200, -100);
    assert(rect3.width() == 0); // Should clamp to 0
    assert(rect3.height() == 0); // Should clamp to 0
    
    std::cout << "overflow handling: PASSED" << std::endl;
}

int main() {
    std::cout << "Running Rect transformation method tests..." << std::endl;
    
    test_translate_methods();
    test_moveTo_methods();
    test_resize_methods();
    test_adjust_methods();
    test_overflow_handling();
    
    std::cout << "All transformation tests PASSED!" << std::endl;
    return 0;
}