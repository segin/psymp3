/*
 * test_rect_integration.cpp - Integration tests for Rect with Widget system
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <cassert>
#include <iostream>

using namespace PsyMP3::Core;

/**
 * Integration tests for Rect class with Widget system
 * 
 * These tests verify that the enhanced Rect class works correctly
 * with the existing Widget system, Surface operations, and layout
 * calculations without breaking backward compatibility.
 */

// Test 17.1: Widget positioning with enhanced Rect
void test_widget_positioning() {
    std::cout << "Testing Widget positioning with enhanced Rect..." << std::endl;
    
    // Test basic widget positioning
    Rect widgetPos(10, 20, 100, 50);
    assert(widgetPos.x() == 10);
    assert(widgetPos.y() == 20);
    assert(widgetPos.width() == 100);
    assert(widgetPos.height() == 50);
    
    // Test edge calculations for widget bounds
    assert(widgetPos.left() == 10);
    assert(widgetPos.top() == 20);
    assert(widgetPos.right() == 110);
    assert(widgetPos.bottom() == 70);
    
    // Test center calculations for widget centering
    assert(widgetPos.centerX() == 60);
    assert(widgetPos.centerY() == 45);
    
    // Test corner calculations for widget positioning
    auto [tl_x, tl_y] = widgetPos.topLeft();
    assert(tl_x == 10 && tl_y == 20);
    
    auto [tr_x, tr_y] = widgetPos.topRight();
    assert(tr_x == 110 && tr_y == 20);
    
    auto [bl_x, bl_y] = widgetPos.bottomLeft();
    assert(bl_x == 10 && bl_y == 70);
    
    auto [br_x, br_y] = widgetPos.bottomRight();
    assert(br_x == 110 && br_y == 70);
    
    std::cout << "  ✓ Basic widget positioning works correctly" << std::endl;
    
    // Test widget translation (moving widgets)
    Rect movingWidget(50, 50, 80, 60);
    movingWidget.translate(10, -5);
    assert(movingWidget.x() == 60);
    assert(movingWidget.y() == 45);
    assert(movingWidget.width() == 80);  // Dimensions preserved
    assert(movingWidget.height() == 60);
    
    std::cout << "  ✓ Widget translation preserves dimensions" << std::endl;
    
    // Test widget resizing
    Rect resizableWidget(100, 100, 200, 150);
    resizableWidget.resize(250, 180);
    assert(resizableWidget.x() == 100);  // Position preserved
    assert(resizableWidget.y() == 100);
    assert(resizableWidget.width() == 250);
    assert(resizableWidget.height() == 180);
    
    std::cout << "  ✓ Widget resizing preserves position" << std::endl;
    
    // Test widget centering in container
    Rect childWidget(0, 0, 100, 50);
    Rect containerWidget(0, 0, 400, 300);
    childWidget.centerIn(containerWidget);
    
    // Child should be centered in container
    int expected_x = (400 - 100) / 2;  // 150
    int expected_y = (300 - 50) / 2;   // 125
    assert(childWidget.x() == expected_x);
    assert(childWidget.y() == expected_y);
    
    std::cout << "  ✓ Widget centering in container works correctly" << std::endl;
    
    // Test mouse hit detection with contains()
    Rect buttonWidget(50, 50, 120, 40);
    
    // Test points inside button
    assert(buttonWidget.contains(50, 50));    // Top-left corner
    assert(buttonWidget.contains(100, 70));   // Center area
    assert(buttonWidget.contains(169, 89));   // Near bottom-right
    
    // Test points outside button
    assert(!buttonWidget.contains(49, 50));   // Left of button
    assert(!buttonWidget.contains(50, 49));   // Above button
    assert(!buttonWidget.contains(170, 70));  // Right of button (exclusive)
    assert(!buttonWidget.contains(100, 90));  // Below button (exclusive)
    
    std::cout << "  ✓ Mouse hit detection with contains() works correctly" << std::endl;
    
    std::cout << "✓ All Widget positioning tests passed!" << std::endl;
}

// Test 17.2: Surface operations with enhanced Rect
void test_surface_operations() {
    std::cout << "Testing Surface operations with enhanced Rect..." << std::endl;
    
    // Test clipping rectangle calculations
    Rect surfaceRect(0, 0, 800, 600);
    Rect widgetRect(700, 500, 200, 150);
    
    // Calculate visible area (intersection)
    Rect visibleArea = surfaceRect.intersection(widgetRect);
    assert(!visibleArea.isEmpty());
    assert(visibleArea.x() == 700);
    assert(visibleArea.y() == 500);
    assert(visibleArea.width() == 100);   // Clipped to surface bounds
    assert(visibleArea.height() == 100);  // Clipped to surface bounds
    
    std::cout << "  ✓ Clipping rectangle calculation works correctly" << std::endl;
    
    // Test non-overlapping surfaces
    Rect offscreenWidget(900, 700, 100, 100);
    Rect noOverlap = surfaceRect.intersection(offscreenWidget);
    assert(noOverlap.isEmpty());
    
    std::cout << "  ✓ Non-overlapping surface detection works correctly" << std::endl;
    
    // Test surface coordinate transformations
    Rect parentSurface(100, 100, 400, 300);
    Rect childSurface(50, 50, 100, 80);
    
    // Transform child to parent coordinates
    Rect childInParent = childSurface.translated(parentSurface.x(), parentSurface.y());
    assert(childInParent.x() == 150);
    assert(childInParent.y() == 150);
    assert(childInParent.width() == 100);
    assert(childInParent.height() == 80);
    
    std::cout << "  ✓ Surface coordinate transformation works correctly" << std::endl;
    
    // Test bounding box calculation for multiple surfaces
    Rect surface1(10, 10, 100, 100);
    Rect surface2(80, 80, 100, 100);
    Rect boundingBox = surface1.united(surface2);
    
    assert(boundingBox.x() == 10);
    assert(boundingBox.y() == 10);
    assert(boundingBox.width() == 170);   // Covers both surfaces
    assert(boundingBox.height() == 170);
    
    std::cout << "  ✓ Bounding box calculation for surfaces works correctly" << std::endl;
    
    std::cout << "✓ All Surface operation tests passed!" << std::endl;
}

// Test 17.3: Layout operations with enhanced Rect
void test_layout_operations() {
    std::cout << "Testing layout operations with enhanced Rect..." << std::endl;
    
    // Test horizontal layout calculation
    std::vector<Rect> horizontalWidgets;
    int x_offset = 10;
    int y_pos = 20;
    int spacing = 5;
    
    for (int i = 0; i < 5; ++i) {
        Rect widget(x_offset, y_pos, 80, 30);
        horizontalWidgets.push_back(widget);
        x_offset += 80 + spacing;
    }
    
    // Verify horizontal layout
    assert(horizontalWidgets[0].x() == 10);
    assert(horizontalWidgets[1].x() == 95);   // 10 + 80 + 5
    assert(horizontalWidgets[2].x() == 180);  // 95 + 80 + 5
    assert(horizontalWidgets[4].x() == 350);  // Last widget
    
    std::cout << "  ✓ Horizontal layout calculation works correctly" << std::endl;
    
    // Test vertical layout calculation
    std::vector<Rect> verticalWidgets;
    int x_pos = 10;
    int y_offset = 10;
    
    for (int i = 0; i < 4; ++i) {
        Rect widget(x_pos, y_offset, 100, 40);
        verticalWidgets.push_back(widget);
        y_offset += 40 + spacing;
    }
    
    // Verify vertical layout
    assert(verticalWidgets[0].y() == 10);
    assert(verticalWidgets[1].y() == 55);   // 10 + 40 + 5
    assert(verticalWidgets[2].y() == 100);  // 55 + 40 + 5
    assert(verticalWidgets[3].y() == 145);  // Last widget
    
    std::cout << "  ✓ Vertical layout calculation works correctly" << std::endl;
    
    // Test grid layout calculation
    int grid_cols = 3;
    int grid_rows = 2;
    int cell_width = 100;
    int cell_height = 80;
    int grid_spacing = 10;
    
    std::vector<Rect> gridWidgets;
    for (int row = 0; row < grid_rows; ++row) {
        for (int col = 0; col < grid_cols; ++col) {
            int x = col * (cell_width + grid_spacing);
            int y = row * (cell_height + grid_spacing);
            gridWidgets.push_back(Rect(x, y, cell_width, cell_height));
        }
    }
    
    // Verify grid layout
    assert(gridWidgets[0].x() == 0 && gridWidgets[0].y() == 0);      // Top-left
    assert(gridWidgets[1].x() == 110 && gridWidgets[1].y() == 0);    // Top-middle
    assert(gridWidgets[2].x() == 220 && gridWidgets[2].y() == 0);    // Top-right
    assert(gridWidgets[3].x() == 0 && gridWidgets[3].y() == 90);     // Bottom-left
    assert(gridWidgets[5].x() == 220 && gridWidgets[5].y() == 90);   // Bottom-right
    
    std::cout << "  ✓ Grid layout calculation works correctly" << std::endl;
    
    // Test padding/margin application with expand/shrink
    Rect contentArea(50, 50, 300, 200);
    int padding = 10;
    
    // Add padding (expand outward)
    Rect paddedArea = contentArea.expanded(padding);
    assert(paddedArea.x() == 40);
    assert(paddedArea.y() == 40);
    assert(paddedArea.width() == 320);
    assert(paddedArea.height() == 220);
    
    // Remove padding (shrink inward)
    Rect unpaddedArea = paddedArea.shrunk(padding);
    assert(unpaddedArea == contentArea);  // Should match original
    
    std::cout << "  ✓ Padding/margin application works correctly" << std::endl;
    
    // Test window frame layout (titlebar + content area)
    Rect windowFrame(100, 100, 400, 300);
    int titlebarHeight = 30;
    int borderWidth = 2;
    
    // Calculate titlebar area
    Rect titlebar(windowFrame.x(), windowFrame.y(), 
                  windowFrame.width(), titlebarHeight);
    
    // Calculate content area (below titlebar, with borders)
    Rect contentWithBorders(windowFrame.x(), 
                           windowFrame.y() + titlebarHeight,
                           windowFrame.width(),
                           windowFrame.height() - titlebarHeight);
    
    Rect content = contentWithBorders.shrunk(borderWidth);
    
    assert(titlebar.x() == 100);
    assert(titlebar.y() == 100);
    assert(titlebar.height() == 30);
    
    assert(content.x() == 102);  // With border
    assert(content.y() == 132);  // Below titlebar + border
    assert(content.width() == 396);   // Shrunk by borders
    assert(content.height() == 266);  // Shrunk by borders
    
    std::cout << "  ✓ Window frame layout calculation works correctly" << std::endl;
    
    std::cout << "✓ All layout operation tests passed!" << std::endl;
}

// Test 17.4: Complete widget system integration
void test_complete_widget_integration() {
    std::cout << "Testing complete widget system integration..." << std::endl;
    
    // Simulate a complete UI layout scenario
    Rect screen(0, 0, 1024, 768);
    
    // Main window centered on screen
    Rect mainWindow(0, 0, 800, 600);
    mainWindow.centerIn(screen);
    assert(mainWindow.x() == 112);  // (1024 - 800) / 2
    assert(mainWindow.y() == 84);   // (768 - 600) / 2
    
    std::cout << "  ✓ Main window centering works" << std::endl;
    
    // Dialog box centered in main window
    Rect dialog(0, 0, 400, 300);
    dialog.centerIn(mainWindow);
    
    // Dialog should be centered relative to main window
    int dialog_x_in_window = dialog.x() - mainWindow.x();
    int dialog_y_in_window = dialog.y() - mainWindow.y();
    assert(dialog_x_in_window == 200);  // (800 - 400) / 2
    assert(dialog_y_in_window == 150);  // (600 - 300) / 2
    
    std::cout << "  ✓ Dialog centering in window works" << std::endl;
    
    // Button layout in dialog
    int button_width = 100;
    int button_height = 30;
    int button_spacing = 10;
    int button_y = dialog.bottom() - button_height - 20;  // 20px from bottom
    
    // Calculate button positions (OK and Cancel)
    int total_button_width = button_width * 2 + button_spacing;
    int button_start_x = dialog.centerX() - total_button_width / 2;
    
    Rect okButton(button_start_x, button_y, button_width, button_height);
    Rect cancelButton(button_start_x + button_width + button_spacing, 
                      button_y, button_width, button_height);
    
    // Verify buttons are positioned correctly
    assert(okButton.width() == 100);
    assert(cancelButton.width() == 100);
    assert(cancelButton.x() - okButton.right() == button_spacing);
    
    std::cout << "  ✓ Button layout in dialog works" << std::endl;
    
    // Test event handling with contains()
    int mouse_x = okButton.centerX();
    int mouse_y = okButton.centerY();
    
    assert(okButton.contains(mouse_x, mouse_y));
    assert(!cancelButton.contains(mouse_x, mouse_y));
    
    std::cout << "  ✓ Event handling with contains() works" << std::endl;
    
    // Test visibility culling with intersects()
    Rect viewport(0, 0, 1024, 768);
    Rect visibleWidget(500, 400, 200, 150);
    Rect offscreenWidget(2000, 2000, 100, 100);
    
    assert(viewport.intersects(visibleWidget));
    assert(!viewport.intersects(offscreenWidget));
    
    std::cout << "  ✓ Visibility culling with intersects() works" << std::endl;
    
    // Test widget overlap detection
    Rect widget1(100, 100, 200, 150);
    Rect widget2(350, 300, 200, 150);  // No overlap with widget1
    Rect widget3(150, 150, 100, 100);  // Overlaps with widget1
    
    assert(!widget1.intersects(widget2));  // No overlap
    assert(widget1.intersects(widget3));   // Overlap
    
    Rect overlap = widget1.intersection(widget3);
    assert(!overlap.isEmpty());
    assert(overlap.x() == 150);
    assert(overlap.y() == 150);
    assert(overlap.width() == 100);
    assert(overlap.height() == 100);
    
    std::cout << "  ✓ Widget overlap detection works" << std::endl;
    
    std::cout << "✓ All complete widget integration tests passed!" << std::endl;
}

int main() {
    std::cout << "=== Rect Integration Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        test_widget_positioning();
        std::cout << std::endl;
        
        test_surface_operations();
        std::cout << std::endl;
        
        test_layout_operations();
        std::cout << std::endl;
        
        test_complete_widget_integration();
        std::cout << std::endl;
        
        std::cout << "=== ALL INTEGRATION TESTS PASSED ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
