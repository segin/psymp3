/*
 * test_rect_modern_cpp.cpp - Test modern C++ features of Rect class
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include <iostream>
#include <cassert>

int main()
{
    std::cout << "Testing Rect modern C++ features..." << std::endl;
    
    // Test comparison operators
    Rect rect1(10, 20, 100, 200);
    Rect rect2(10, 20, 100, 200);
    Rect rect3(15, 25, 100, 200);
    
    // Test equality operator
    assert(rect1 == rect2);
    assert(!(rect1 == rect3));
    std::cout << "✓ Equality operator works correctly" << std::endl;
    
    // Test inequality operator
    assert(!(rect1 != rect2));
    assert(rect1 != rect3);
    std::cout << "✓ Inequality operator works correctly" << std::endl;
    
    // Test toString method
    std::string str1 = rect1.toString();
    std::string expected1 = "Rect(10, 20, 100, 200)";
    assert(str1 == expected1);
    std::cout << "✓ toString() for valid rectangle: " << str1 << std::endl;
    
    // Test toString with empty rectangle
    Rect emptyRect(0, 0, 0, 100);
    std::string str2 = emptyRect.toString();
    std::cout << "✓ toString() for empty rectangle: " << str2 << std::endl;
    assert(str2.find("[EMPTY]") != std::string::npos);
    
    // Test toString with invalid rectangle (zero height)
    Rect invalidRect(10, 10, 100, 0);
    std::string str3 = invalidRect.toString();
    std::cout << "✓ toString() for invalid rectangle: " << str3 << std::endl;
    assert(str3.find("[EMPTY]") != std::string::npos);
    
    std::cout << "All modern C++ feature tests passed!" << std::endl;
    return 0;
}