/*
 * test_rect_centering.cpp - Test centering operations for Rect class
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <cassert>

// Minimal Rect class definition for testing
#include <utility>
#include <cstdint>
#include <algorithm>

class Rect
{
    public:
        Rect() : m_x(0), m_y(0), m_width(0), m_height(0) {}
        Rect(int16_t x, int16_t y, uint16_t w, uint16_t h) : m_x(x), m_y(y), m_width(w), m_height(h) {}
        Rect(uint16_t w, uint16_t h) : m_x(0), m_y(0), m_width(w), m_height(h) {}
        
        // Accessor methods
        int16_t x() const { return m_x; }
        int16_t y() const { return m_y; }
        uint16_t width() const { return m_width; }
        uint16_t height() const { return m_height; }
        
        // Center point calculation methods
        int16_t centerX() const { return m_x + m_width / 2; }
        int16_t centerY() const { return m_y + m_height / 2; }
        
        // Centering operations
        void centerIn(const Rect& container);
        Rect centeredIn(const Rect& container) const;
        
    private:
        int16_t m_x;
        int16_t m_y;
        uint16_t m_width;
        uint16_t m_height;
};

// Implementation of centering methods
void Rect::centerIn(const Rect& container)
{
    // Calculate the center point of the container
    int16_t container_center_x = container.centerX();
    int16_t container_center_y = container.centerY();
    
    // Calculate the new position to center this rectangle
    // Position = container_center - (our_size / 2)
    int32_t new_x = static_cast<int32_t>(container_center_x) - static_cast<int32_t>(m_width) / 2;
    int32_t new_y = static_cast<int32_t>(container_center_y) - static_cast<int32_t>(m_height) / 2;
    
    // Handle coordinate overflow/underflow
    if (new_x < -32768) new_x = -32768;
    if (new_x > 32767) new_x = 32767;
    if (new_y < -32768) new_y = -32768;
    if (new_y > 32767) new_y = 32767;
    
    // Update position
    m_x = static_cast<int16_t>(new_x);
    m_y = static_cast<int16_t>(new_y);
}

Rect Rect::centeredIn(const Rect& container) const
{
    Rect result(*this);
    result.centerIn(container);
    return result;
}

// Test functions
void test_basic_centering()
{
    std::cout << "Testing basic centering..." << std::endl;
    
    // Test centering a 10x10 rectangle in a 100x100 container at origin
    Rect container(0, 0, 100, 100);  // Container at (0,0) with size 100x100
    Rect rect(0, 0, 10, 10);         // Rectangle at (0,0) with size 10x10
    
    rect.centerIn(container);
    
    // Container center is at (50, 50)
    // Rectangle should be positioned at (45, 45) to center it
    assert(rect.x() == 45);
    assert(rect.y() == 45);
    assert(rect.width() == 10);
    assert(rect.height() == 10);
    
    std::cout << "Basic centering test passed!" << std::endl;
}

void test_centering_with_offset_container()
{
    std::cout << "Testing centering with offset container..." << std::endl;
    
    // Test centering in a container that's not at origin
    Rect container(20, 30, 60, 40);  // Container at (20,30) with size 60x40
    Rect rect(0, 0, 20, 10);         // Rectangle at (0,0) with size 20x10
    
    rect.centerIn(container);
    
    // Container center is at (20 + 60/2, 30 + 40/2) = (50, 50)
    // Rectangle should be positioned at (50 - 20/2, 50 - 10/2) = (40, 45)
    assert(rect.x() == 40);
    assert(rect.y() == 45);
    assert(rect.width() == 20);
    assert(rect.height() == 10);
    
    std::cout << "Offset container centering test passed!" << std::endl;
}

void test_centering_larger_rectangle()
{
    std::cout << "Testing centering rectangle larger than container..." << std::endl;
    
    // Test centering a rectangle that's larger than the container
    Rect container(10, 10, 50, 50);  // Container at (10,10) with size 50x50
    Rect rect(0, 0, 100, 80);        // Rectangle at (0,0) with size 100x80
    
    rect.centerIn(container);
    
    // Container center is at (10 + 50/2, 10 + 50/2) = (35, 35)
    // Rectangle should be positioned at (35 - 100/2, 35 - 80/2) = (-15, -5)
    assert(rect.x() == -15);
    assert(rect.y() == -5);
    assert(rect.width() == 100);
    assert(rect.height() == 80);
    
    std::cout << "Large rectangle centering test passed!" << std::endl;
}

void test_centered_in_method()
{
    std::cout << "Testing centeredIn method..." << std::endl;
    
    // Test the const version that returns a new rectangle
    Rect container(0, 0, 100, 100);
    Rect original(5, 5, 20, 20);
    
    Rect centered = original.centeredIn(container);
    
    // Original should be unchanged
    assert(original.x() == 5);
    assert(original.y() == 5);
    assert(original.width() == 20);
    assert(original.height() == 20);
    
    // Centered should be positioned at (40, 40) to center the 20x20 rect in 100x100 container
    assert(centered.x() == 40);
    assert(centered.y() == 40);
    assert(centered.width() == 20);
    assert(centered.height() == 20);
    
    std::cout << "centeredIn method test passed!" << std::endl;
}

void test_edge_cases()
{
    std::cout << "Testing edge cases..." << std::endl;
    
    // Test with zero-size rectangle
    Rect container(0, 0, 100, 100);
    Rect zero_rect(10, 10, 0, 0);
    
    zero_rect.centerIn(container);
    
    // Zero-size rectangle should be centered at container center
    assert(zero_rect.x() == 50);
    assert(zero_rect.y() == 50);
    assert(zero_rect.width() == 0);
    assert(zero_rect.height() == 0);
    
    // Test with zero-size container
    Rect zero_container(25, 25, 0, 0);
    Rect rect(0, 0, 10, 10);
    
    rect.centerIn(zero_container);
    
    // Rectangle should be centered at container position
    assert(rect.x() == 20);  // 25 - 10/2 = 20
    assert(rect.y() == 20);  // 25 - 10/2 = 20
    
    std::cout << "Edge cases test passed!" << std::endl;
}

int main()
{
    std::cout << "Running Rect centering tests..." << std::endl;
    
    test_basic_centering();
    test_centering_with_offset_container();
    test_centering_larger_rectangle();
    test_centered_in_method();
    test_edge_cases();
    
    std::cout << "All centering tests passed!" << std::endl;
    return 0;
}