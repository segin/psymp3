/*
 * test_rect_backward_compatibility.cpp - Test backward compatibility of enhanced Rect class
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/core/rect.h"
using PsyMP3::Core::Rect;
#include <iostream>
#include <cassert>

using namespace TestFramework;

/**
 * Test that all existing Rect usage patterns continue to work
 */
void test_existing_constructors() {
    // Test default constructor
    Rect default_rect;
    assert(default_rect.x() == 0);
    assert(default_rect.y() == 0);
    assert(default_rect.width() == 0);
    assert(default_rect.height() == 0);
    
    // Test size-only constructor (used in some places)
    Rect size_rect(100, 50);
    assert(size_rect.x() == 0);
    assert(size_rect.y() == 0);
    assert(size_rect.width() == 100);
    assert(size_rect.height() == 50);
    
    // Test full constructor (most common usage)
    Rect full_rect(10, 20, 100, 50);
    assert(full_rect.x() == 10);
    assert(full_rect.y() == 20);
    assert(full_rect.width() == 100);
    assert(full_rect.height() == 50);
}

/**
 * Test that all existing getter/setter patterns work
 */
void test_existing_accessors() {
    Rect rect(10, 20, 100, 50);
    
    // Test getters
    assert(rect.x() == 10);
    assert(rect.y() == 20);
    assert(rect.width() == 100);
    assert(rect.height() == 50);
    
    // Test setters
    rect.x(15);
    rect.y(25);
    rect.width(120);
    rect.height(60);
    
    assert(rect.x() == 15);
    assert(rect.y() == 25);
    assert(rect.width() == 120);
    assert(rect.height() == 60);
}

/**
 * Test patterns used in player.cpp and other UI code
 */
void test_ui_usage_patterns() {
    // Pattern: Rect(0,0,0,0) for empty/placeholder rectangles
    Rect empty_rect(0, 0, 0, 0);
    assert(empty_rect.x() == 0);
    assert(empty_rect.y() == 0);
    assert(empty_rect.width() == 0);
    assert(empty_rect.height() == 0);
    
    // Pattern: Rect(x, y, width, height) for positioning widgets
    Rect widget_rect(399, 370, 222, 16);
    assert(widget_rect.x() == 399);
    assert(widget_rect.y() == 370);
    assert(widget_rect.width() == 222);
    assert(widget_rect.height() == 16);
    
    // Pattern: Rect(0, 0, width, height) for surfaces
    Rect surface_rect(0, 0, 640, 350);
    assert(surface_rect.x() == 0);
    assert(surface_rect.y() == 0);
    assert(surface_rect.width() == 640);
    assert(surface_rect.height() == 350);
}

/**
 * Test that copy semantics work as expected
 */
void test_copy_semantics() {
    Rect original(10, 20, 100, 50);
    
    // Copy constructor
    Rect copy1(original);
    assert(copy1.x() == 10);
    assert(copy1.y() == 20);
    assert(copy1.width() == 100);
    assert(copy1.height() == 50);
    
    // Assignment operator
    Rect copy2;
    copy2 = original;
    assert(copy2.x() == 10);
    assert(copy2.y() == 20);
    assert(copy2.width() == 100);
    assert(copy2.height() == 50);
    
    // Verify independence
    original.x(99);
    assert(copy1.x() == 10);  // Should not change
    assert(copy2.x() == 10);  // Should not change
}

/**
 * Test that the enhanced methods don't interfere with existing usage
 */
void test_enhanced_methods_coexistence() {
    Rect rect(10, 20, 100, 50);
    
    // Original methods should still work
    assert(rect.x() == 10);
    assert(rect.y() == 20);
    assert(rect.width() == 100);
    assert(rect.height() == 50);
    
    // New methods should work alongside
    assert(rect.right() == 110);  // x + width
    assert(rect.bottom() == 70);  // y + height
    assert(rect.area() == 5000);  // width * height
    
    // Original methods should still work after using new ones
    assert(rect.x() == 10);
    assert(rect.y() == 20);
    assert(rect.width() == 100);
    assert(rect.height() == 50);
}

int main() {
    TestSuite suite("Rectangle Backward Compatibility Tests");
    
    suite.addTest("Existing Constructors", test_existing_constructors);
    suite.addTest("Existing Accessors", test_existing_accessors);
    suite.addTest("UI Usage Patterns", test_ui_usage_patterns);
    suite.addTest("Copy Semantics", test_copy_semantics);
    suite.addTest("Enhanced Methods Coexistence", test_enhanced_methods_coexistence);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}