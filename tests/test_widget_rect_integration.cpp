/*
 * test_widget_rect_integration.cpp - Integration test for Rect with Widget system
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "test_framework.h"
#include "test_rect_utilities.h"
#include "../include/rect.h"
#include <iostream>

using namespace TestFramework;

/**
 * Test typical Widget positioning patterns
 */
void test_widget_positioning_patterns() {
    // Pattern 1: Create widget at specific position (like in player.cpp)
    Rect widget_pos(399, 370, 222, 16);
    ASSERT_TRUE(widget_pos.x() == 399, "Widget X position should be set correctly");
    ASSERT_TRUE(widget_pos.y() == 370, "Widget Y position should be set correctly");
    ASSERT_TRUE(widget_pos.width() == 222, "Widget width should be set correctly");
    ASSERT_TRUE(widget_pos.height() == 16, "Widget height should be set correctly");
    
    // Pattern 2: Create full-screen widget (like spectrum analyzer)
    Rect fullscreen(0, 0, 640, 350);
    ASSERT_TRUE(fullscreen.x() == 0, "Fullscreen X should be 0");
    ASSERT_TRUE(fullscreen.y() == 0, "Fullscreen Y should be 0");
    ASSERT_TRUE(fullscreen.width() == 640, "Fullscreen width should be 640");
    ASSERT_TRUE(fullscreen.height() == 350, "Fullscreen height should be 350");
    
    // Pattern 3: Empty/placeholder rectangle
    Rect placeholder(0, 0, 0, 0);
    ASSERT_TRUE(placeholder.isEmpty(), "Placeholder should be empty");
    ASSERT_TRUE(!placeholder.isValid(), "Placeholder should be invalid");
}

/**
 * Test mouse hit testing patterns (critical for UI interaction)
 */
void test_mouse_hit_testing() {
    // Create a button-like widget
    Rect button(100, 50, 80, 25);
    
    // Test points inside the button
    ASSERT_TRUE(button.contains(100, 50), "Top-left corner should be inside");
    ASSERT_TRUE(button.contains(140, 62), "Center should be inside");
    ASSERT_TRUE(button.contains(179, 74), "Bottom-right corner should be inside");
    
    // Test points outside the button
    ASSERT_TRUE(!button.contains(99, 50), "Point left of button should be outside");
    ASSERT_TRUE(!button.contains(100, 49), "Point above button should be outside");
    ASSERT_TRUE(!button.contains(180, 62), "Point right of button should be outside");
    ASSERT_TRUE(!button.contains(140, 75), "Point below button should be outside");
    
    // Test edge cases
    ASSERT_TRUE(!button.contains(180, 75), "Bottom-right exclusive boundary");
}

/**
 * Test layout calculations (like centering widgets)
 */
void test_layout_calculations() {
    // Container (like screen or parent widget)
    Rect container(0, 0, 640, 480);
    
    // Widget to center
    Rect widget(0, 0, 200, 100);
    
    // Calculate center position manually (like in player.cpp)
    int center_x = (container.width() - widget.width()) / 2;
    int center_y = (container.height() - widget.height()) / 2;
    
    widget.x(center_x);
    widget.y(center_y);
    
    ASSERT_TRUE(widget.x() == 220, "Centered X should be 220");
    ASSERT_TRUE(widget.y() == 190, "Centered Y should be 190");
    
    // Test using new centerIn method
    Rect widget2(0, 0, 200, 100);
    widget2.centerIn(container);
    
    ASSERT_TRUE(widget2.x() == widget.x(), "centerIn should match manual calculation");
    ASSERT_TRUE(widget2.y() == widget.y(), "centerIn should match manual calculation");
}

/**
 * Test Surface blitting rectangle patterns
 */
void test_surface_blitting_patterns() {
    // Pattern 1: Full surface blit (like in player.cpp)
    Rect source_rect(0, 0, 640, 350);
    ASSERT_TRUE(source_rect.area() == 224000, "Source area should be correct");
    
    // Pattern 2: Partial surface blit
    Rect clip_rect(10, 10, 100, 50);
    Rect intersection = source_rect.intersection(clip_rect);
    
    ASSERT_TRUE(intersection.x() == 10, "Intersection X should be 10");
    ASSERT_TRUE(intersection.y() == 10, "Intersection Y should be 10");
    ASSERT_TRUE(intersection.width() == 100, "Intersection width should be 100");
    ASSERT_TRUE(intersection.height() == 50, "Intersection height should be 50");
    
    // Pattern 3: No intersection (clipped out)
    Rect offscreen(-100, -100, 50, 50);
    Rect no_intersection = source_rect.intersection(offscreen);
    ASSERT_TRUE(no_intersection.isEmpty(), "No intersection should be empty");
}

/**
 * Test coordinate transformations (like widget movement)
 */
void test_coordinate_transformations() {
    // Start with a widget at origin
    Rect widget(0, 0, 100, 50);
    
    // Move it (like animation or drag-and-drop)
    widget.translate(50, 25);
    ASSERT_TRUE(widget.x() == 50, "Translated X should be 50");
    ASSERT_TRUE(widget.y() == 25, "Translated Y should be 25");
    ASSERT_TRUE(widget.width() == 100, "Width should remain unchanged");
    ASSERT_TRUE(widget.height() == 50, "Height should remain unchanged");
    
    // Move to absolute position (like repositioning)
    widget.moveTo(200, 150);
    ASSERT_TRUE(widget.x() == 200, "Moved X should be 200");
    ASSERT_TRUE(widget.y() == 150, "Moved Y should be 150");
    
    // Resize (like window resize)
    widget.resize(150, 75);
    ASSERT_TRUE(widget.width() == 150, "Resized width should be 150");
    ASSERT_TRUE(widget.height() == 75, "Resized height should be 75");
    ASSERT_TRUE(widget.x() == 200, "X should remain unchanged");
    ASSERT_TRUE(widget.y() == 150, "Y should remain unchanged");
}

int main() {
    TestSuite suite("Widget-Rect Integration Tests");
    
    suite.addTest("Widget Positioning Patterns", test_widget_positioning_patterns);
    suite.addTest("Mouse Hit Testing", test_mouse_hit_testing);
    suite.addTest("Layout Calculations", test_layout_calculations);
    suite.addTest("Surface Blitting Patterns", test_surface_blitting_patterns);
    suite.addTest("Coordinate Transformations", test_coordinate_transformations);
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print comprehensive results
    suite.printResults(results);
    
    // Return appropriate exit code
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}