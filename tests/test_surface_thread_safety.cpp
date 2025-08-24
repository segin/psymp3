/*
 * test_surface_thread_safety.cpp - Thread safety tests for Surface class
 * This file is part of PsyMP3.
 * Copyright © 2024 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Test framework includes
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <cassert>
#include <iostream>

class SurfaceThreadSafetyTest {
private:
    static constexpr int TEST_SURFACE_WIDTH = 320;
    static constexpr int TEST_SURFACE_HEIGHT = 240;
    static constexpr int NUM_THREADS = 8;
    static constexpr int OPERATIONS_PER_THREAD = 100;
    
    std::atomic<int> m_error_count{0};
    std::atomic<int> m_completed_operations{0};
    
public:
    void runAllTests() {
        std::cout << "Running Surface thread safety tests...\n";
        
        testConcurrentPixelDrawing();
        testConcurrentLineDrawing();
        testConcurrentShapeDrawing();
        testConcurrentComplexOperations();
        testSDLLockingConsistency();
        testPerformanceRegression();
        
        std::cout << "All Surface thread safety tests completed.\n";
    }
    
private:
    void testConcurrentPixelDrawing() {
        std::cout << "Testing concurrent pixel drawing...\n";
        
        auto surface = std::make_unique<Surface>(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT);
        if (!surface->isValid()) {
            std::cerr << "Failed to create test surface\n";
            return;
        }
        
        m_error_count = 0;
        m_completed_operations = 0;
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([this, &surface, i]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> x_dist(0, TEST_SURFACE_WIDTH - 1);
                std::uniform_int_distribution<> y_dist(0, TEST_SURFACE_HEIGHT - 1);
                std::uniform_int_distribution<> color_dist(0, 255);
                
                try {
                    for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                        int16_t x = x_dist(gen);
                        int16_t y = y_dist(gen);
                        uint8_t r = color_dist(gen);
                        uint8_t g = color_dist(gen);
                        uint8_t b = color_dist(gen);
                        uint8_t a = 255;
                        
                        surface->pixel(x, y, r, g, b, a);
                        m_completed_operations++;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " error: " << e.what() << "\n";
                    m_error_count++;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        assert(m_error_count == 0);
        assert(m_completed_operations == NUM_THREADS * OPERATIONS_PER_THREAD);
        std::cout << "Concurrent pixel drawing test passed.\n";
    }
    
    void testConcurrentLineDrawing() {
        std::cout << "Testing concurrent line drawing...\n";
        
        auto surface = std::make_unique<Surface>(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT);
        if (!surface->isValid()) {
            std::cerr << "Failed to create test surface\n";
            return;
        }
        
        m_error_count = 0;
        m_completed_operations = 0;
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([this, &surface, i]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> coord_dist(0, std::min(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT) - 1);
                std::uniform_int_distribution<> color_dist(0, 255);
                
                try {
                    for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                        int16_t x1 = coord_dist(gen);
                        int16_t y1 = coord_dist(gen);
                        int16_t x2 = coord_dist(gen);
                        int16_t y2 = coord_dist(gen);
                        uint8_t r = color_dist(gen);
                        uint8_t g = color_dist(gen);
                        uint8_t b = color_dist(gen);
                        uint8_t a = 255;
                        
                        // Test different line drawing methods
                        switch (j % 4) {
                            case 0:
                                surface->line(x1, y1, x2, y2, r, g, b, a);
                                break;
                            case 1:
                                surface->hline(x1, x2, y1, r, g, b, a);
                                break;
                            case 2:
                                surface->vline(x1, y1, y2, r, g, b, a);
                                break;
                            case 3:
                                surface->rectangle(x1, y1, x2, y2, r, g, b, a);
                                break;
                        }
                        
                        m_completed_operations++;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " error: " << e.what() << "\n";
                    m_error_count++;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        assert(m_error_count == 0);
        assert(m_completed_operations == NUM_THREADS * OPERATIONS_PER_THREAD);
        std::cout << "Concurrent line drawing test passed.\n";
    }
    
    void testConcurrentShapeDrawing() {
        std::cout << "Testing concurrent shape drawing...\n";
        
        auto surface = std::make_unique<Surface>(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT);
        if (!surface->isValid()) {
            std::cerr << "Failed to create test surface\n";
            return;
        }
        
        m_error_count = 0;
        m_completed_operations = 0;
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([this, &surface, i]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> coord_dist(10, std::min(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT) - 10);
                std::uniform_int_distribution<> size_dist(5, 20);
                std::uniform_int_distribution<> color_dist(0, 255);
                
                try {
                    for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                        int16_t x = coord_dist(gen);
                        int16_t y = coord_dist(gen);
                        int16_t size = size_dist(gen);
                        uint8_t r = color_dist(gen);
                        uint8_t g = color_dist(gen);
                        uint8_t b = color_dist(gen);
                        uint8_t a = 255;
                        
                        // Test different shape drawing methods
                        switch (j % 5) {
                            case 0:
                                surface->box(x - size/2, y - size/2, x + size/2, y + size/2, r, g, b, a);
                                break;
                            case 1:
                                surface->filledCircleRGBA(x, y, size/2, r, g, b, a);
                                break;
                            case 2:
                                surface->filledTriangle(x - size/2, y + size/2, x, y - size/2, x + size/2, y + size/2, r, g, b, a);
                                break;
                            case 3:
                                surface->roundedBoxRGBA(x - size/2, y - size/2, x + size/2, y + size/2, size/4, r, g, b, a);
                                break;
                            case 4: {
                                // Test polygon drawing
                                Sint16 vx[4] = {static_cast<Sint16>(x - size/2), static_cast<Sint16>(x + size/2), 
                                               static_cast<Sint16>(x + size/2), static_cast<Sint16>(x - size/2)};
                                Sint16 vy[4] = {static_cast<Sint16>(y - size/2), static_cast<Sint16>(y - size/2), 
                                               static_cast<Sint16>(y + size/2), static_cast<Sint16>(y + size/2)};
                                surface->filledPolygon(vx, vy, 4, r, g, b, a);
                                break;
                            }
                        }
                        
                        m_completed_operations++;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " error: " << e.what() << "\n";
                    m_error_count++;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        assert(m_error_count == 0);
        assert(m_completed_operations == NUM_THREADS * OPERATIONS_PER_THREAD);
        std::cout << "Concurrent shape drawing test passed.\n";
    }
    
    void testConcurrentComplexOperations() {
        std::cout << "Testing concurrent complex operations...\n";
        
        auto surface = std::make_unique<Surface>(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT);
        if (!surface->isValid()) {
            std::cerr << "Failed to create test surface\n";
            return;
        }
        
        m_error_count = 0;
        m_completed_operations = 0;
        
        std::vector<std::thread> threads;
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([this, &surface, i]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> coord_dist(20, std::min(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT) - 20);
                std::uniform_int_distribution<> color_dist(0, 255);
                
                try {
                    for (int j = 0; j < OPERATIONS_PER_THREAD / 2; ++j) { // Fewer operations for complex tests
                        int16_t x = coord_dist(gen);
                        int16_t y = coord_dist(gen);
                        uint8_t r = color_dist(gen);
                        uint8_t g = color_dist(gen);
                        uint8_t b = color_dist(gen);
                        uint8_t a = 255;
                        
                        // Test complex operations
                        switch (j % 2) {
                            case 0: {
                                // Test Bezier curve
                                std::vector<std::pair<double, double>> points = {
                                    {static_cast<double>(x - 10), static_cast<double>(y - 10)},
                                    {static_cast<double>(x), static_cast<double>(y - 20)},
                                    {static_cast<double>(x + 10), static_cast<double>(y - 10)},
                                    {static_cast<double>(x + 10), static_cast<double>(y + 10)}
                                };
                                surface->bezierCurve(points, r, g, b, a, 0.1);
                                break;
                            }
                            case 1: {
                                // Test flood fill (be careful with this one)
                                if (x > 30 && x < TEST_SURFACE_WIDTH - 30 && y > 30 && y < TEST_SURFACE_HEIGHT - 30) {
                                    // First draw a small boundary
                                    surface->rectangle(x - 5, y - 5, x + 5, y + 5, 255, 255, 255, 255);
                                    // Then flood fill inside
                                    surface->floodFill(x, y, r, g, b, a);
                                }
                                break;
                            }
                        }
                        
                        m_completed_operations++;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " error: " << e.what() << "\n";
                    m_error_count++;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        assert(m_error_count == 0);
        std::cout << "Concurrent complex operations test passed.\n";
    }
    
    void testSDLLockingConsistency() {
        std::cout << "Testing SDL locking consistency...\n";
        
        auto surface = std::make_unique<Surface>(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT);
        if (!surface->isValid()) {
            std::cerr << "Failed to create test surface\n";
            return;
        }
        
        m_error_count = 0;
        std::atomic<bool> test_running{true};
        
        // This test verifies that SDL locking doesn't cause deadlocks
        // by having multiple threads perform operations simultaneously
        std::vector<std::thread> threads;
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([this, &surface, &test_running, i]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> coord_dist(0, std::min(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT) - 1);
                std::uniform_int_distribution<> color_dist(0, 255);
                
                int operations = 0;
                try {
                    while (test_running && operations < OPERATIONS_PER_THREAD) {
                        int16_t x = coord_dist(gen);
                        int16_t y = coord_dist(gen);
                        uint8_t r = color_dist(gen);
                        uint8_t g = color_dist(gen);
                        uint8_t b = color_dist(gen);
                        
                        // Mix different operations to test lock consistency
                        surface->pixel(x, y, r, g, b, 255);
                        surface->hline(x, x + 5, y, r, g, b, 255);
                        surface->vline(x, y, y + 5, r, g, b, 255);
                        
                        operations++;
                        
                        // Small delay to increase chance of lock contention
                        std::this_thread::sleep_for(std::chrono::microseconds(1));
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Thread " << i << " locking error: " << e.what() << "\n";
                    m_error_count++;
                }
            });
        }
        
        // Let the test run for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        test_running = false;
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        assert(m_error_count == 0);
        std::cout << "SDL locking consistency test passed.\n";
    }
    
    void testPerformanceRegression() {
        std::cout << "Testing performance regression...\n";
        
        auto surface = std::make_unique<Surface>(TEST_SURFACE_WIDTH, TEST_SURFACE_HEIGHT);
        if (!surface->isValid()) {
            std::cerr << "Failed to create test surface\n";
            return;
        }
        
        const int PERF_OPERATIONS = 10000;
        
        // Test single-threaded performance
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < PERF_OPERATIONS; ++i) {
            surface->pixel(i % TEST_SURFACE_WIDTH, (i / TEST_SURFACE_WIDTH) % TEST_SURFACE_HEIGHT, 255, 0, 0, 255);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "Single-threaded performance: " << PERF_OPERATIONS << " operations in " 
                  << duration.count() << " microseconds\n";
        
        // Test multi-threaded performance
        start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        const int ops_per_thread = PERF_OPERATIONS / NUM_THREADS;
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&surface, ops_per_thread, i]() {
                for (int j = 0; j < ops_per_thread; ++j) {
                    int idx = i * ops_per_thread + j;
                    surface->pixel(idx % TEST_SURFACE_WIDTH, (idx / TEST_SURFACE_WIDTH) % TEST_SURFACE_HEIGHT, 0, 255, 0, 255);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        end_time = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "Multi-threaded performance: " << PERF_OPERATIONS << " operations in " 
                  << duration.count() << " microseconds\n";
        
        std::cout << "Performance regression test completed.\n";
    }
};

int main() {
    // Initialize SDL for testing
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return 1;
    }
    
    try {
        SurfaceThreadSafetyTest test;
        test.runAllTests();
        std::cout << "All Surface thread safety tests passed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        SDL_Quit();
        return 1;
    }
    
    SDL_Quit();
    return 0;
}