/*
 * test_surface_from_bmp_error.cpp - Error path tests for Surface::FromBMP
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <fstream>
#include <cstdio>

using namespace TestFramework;

class TestSurfaceFromBMPError : public TestCase {
public:
    TestSurfaceFromBMPError() : TestCase("Surface::FromBMP Error Paths") {}

    void runTest() override {
        // Test 1: Non-existent file
        TestPatterns::assertThrows<SDLException>([]() {
            Surface::FromBMP("non_existent_file_12345.bmp");
        }, "Could not load BMP", "Should throw SDLException for non-existent file");

        // Test 2: Empty filename
        TestPatterns::assertThrows<SDLException>([]() {
            Surface::FromBMP("");
        }, "Could not load BMP", "Should throw SDLException for empty filename");

        // Test 3: Invalid BMP file (content is not BMP)
        const std::string invalid_bmp = "invalid_test.bmp";
        {
            std::ofstream ofs(invalid_bmp);
            ofs << "This is not a BMP file." << std::endl;
        }

        try {
            TestPatterns::assertThrows<SDLException>([&invalid_bmp]() {
                Surface::FromBMP(invalid_bmp);
            }, "Could not load BMP", "Should throw SDLException for invalid BMP content");
        } catch (...) {
            std::remove(invalid_bmp.c_str());
            throw;
        }

        std::remove(invalid_bmp.c_str());
    }
};

int main() {
    // Initialize SDL for testing
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    TestSuite suite("Surface FromBMP Error Tests");
    suite.addTest(std::make_unique<TestSurfaceFromBMPError>());

    auto results = suite.runAll();
    suite.printResults(results);

    SDL_Quit();
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
