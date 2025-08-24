/*
 * test_surface_performance_regression.cpp - Performance regression test for Surface class
 * This file is part of PsyMP3.
 * Copyright © 2024 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#include <chrono>
#include <iostream>
#include <cassert>

class SurfacePerformanceTest {
private:
    static constexpr int TEST_SURFACE_WIDTH = 640;
    static constexpr int TEST_SURFACE_HEIGHT = 480;
    static constexpr int PERFORMANCE_ITERATIONS = 50000;
    
public:
    void runPerformanceTests() {
        std::cout << "Running Surface performance regression tests...\n";
        
        testPixelDrawingPerformance();
        testLineDrawingPerformance();
        testShapeDrawingPerformance();
        
        std::cout << "Surface performance regression tests completed.\n";
    }
    
private:
    void testPixelDrawingPerformance() {
        std::cout << "Testing pixel drawing performance...\n";
        
        auto surface = std::make_unique<Surface>(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT);
        if (!surface->isValid()) {
            std::cerr << "Failed to create test surface\n";
            return;
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
            int16_t x = i % TEST_SURFACE_WIDTH;
            int16_t y = (i / TEST_SURFACE_WIDTH) % TEST_SURFACE_HEIGHT;
            surface->pixel(x, y, 255, 0, 0, 255);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "Pixel drawing: " << PERFORMANCE_ITERATIONS << " operations in " 
                  << duration.count() << " microseconds\n";
        std::cout << "Average: " << (duration.count() / static_cast<double>(PERFORMANCE_ITERATIONS)) 
                  << " microseconds per pixel\n";
    }
    
    void testLineDrawingPerformance() {
        std::cout << "Testing line drawing performance...\n";
        
        auto surface = std::make_unique<Surface>(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT);
        if (!surface->isValid()) {
            std::cerr << "Failed to create test surface\n";
            return;
        }
        
        const int line_iterations = PERFORMANCE_ITERATIONS / 10; // Fewer iterations for lines
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < line_iterations; ++i) {
            int16_t x1 = (i * 7) % TEST_SURFACE_WIDTH;
            int16_t y1 = (i * 11) % TEST_SURFACE_HEIGHT;
            int16_t x2 = ((i + 50) * 7) % TEST_SURFACE_WIDTH;
            int16_t y2 = ((i + 50) * 11) % TEST_SURFACE_HEIGHT;
            
            switch (i % 3) {
                case 0:
                    surface->line(x1, y1, x2, y2, 255, 0, 0, 255);
                    break;
                case 1:
                    surface->hline(x1, x2, y1, 0, 255, 0, 255);
                    break;
                case 2:
                    surface->vline(x1, y1, y2, 0, 0, 255, 255);
                    break;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "Line drawing: " << line_iterations << " operations in " 
                  << duration.count() << " microseconds\n";
        std::cout << "Average: " << (duration.count() / static_cast<double>(line_iterations)) 
                  << " microseconds per line\n";
    }
    
    void testShapeDrawingPerformance() {
        std::cout << "Testing shape drawing performance...\n";
        
        auto surface = std::make_unique<Surface>(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT);
        if (!surface->isValid()) {
            std::cerr << "Failed to create test surface\n";
            return;
        }
        
        const int shape_iterations = PERFORMANCE_ITERATIONS / 50; // Even fewer iterations for shapes
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < shape_iterations; ++i) {
            int16_t x = 50 + (i * 13) % (TEST_SURFACE_WIDTH - 100);
            int16_t y = 50 + (i * 17) % (TEST_SURFACE_HEIGHT - 100);
            int16_t size = 10 + (i % 20);
            
            switch (i % 4) {
                case 0:
                    surface->box(x - size/2, y - size/2, x + size/2, y + size/2, 255, 0, 0, 255);
                    break;
                case 1:
                    surface->filledCircleRGBA(x, y, size/2, 0, 255, 0, 255);
                    break;
                case 2:
                    surface->rectangle(x - size/2, y - size/2, x + size/2, y + size/2, 0, 0, 255, 255);
                    break;
                case 3:
                    surface->roundedBoxRGBA(x - size/2, y - size/2, x + size/2, y + size/2, size/4, 255, 255, 0, 255);
                    break;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "Shape drawing: " << shape_iterations << " operations in " 
                  << duration.count() << " microseconds\n";
        std::cout << "Average: " << (duration.count() / static_cast<double>(shape_iterations)) 
                  << " microseconds per shape\n";
    }
};

int main() {
    // Initialize SDL for testing
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return 1;
    }
    
    try {
        SurfacePerformanceTest test;
        test.runPerformanceTests();
        std::cout << "Surface performance regression tests completed successfully!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        SDL_Quit();
        return 1;
    }
    
    SDL_Quit();
    return 0;
}