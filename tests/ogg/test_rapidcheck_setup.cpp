/*
 * test_rapidcheck_setup.cpp - RapidCheck setup verification test
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * This test verifies that RapidCheck is properly configured and working.
 */

#include "psymp3.h"

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>
#endif

#include <iostream>
#include <string>

/**
 * @brief Simple property test to verify RapidCheck is working
 * 
 * This test verifies that:
 * 1. RapidCheck library is properly linked
 * 2. Property-based tests can be defined and executed
 * 3. The test infrastructure is ready for OggDemuxer property tests
 */
int main() {
    std::cout << "RapidCheck Setup Verification Test" << std::endl;
    std::cout << "===================================" << std::endl;
    
#ifdef HAVE_RAPIDCHECK
    std::cout << "RapidCheck is available" << std::endl;
    
    // Test 1: Basic property test - integer addition is commutative
    bool test1_passed = rc::check("addition is commutative", [](int a, int b) {
        return a + b == b + a;
    });
    
    if (test1_passed) {
        std::cout << "✓ Test 1: Basic property test passed" << std::endl;
    } else {
        std::cout << "✗ Test 1: Basic property test failed" << std::endl;
        return 1;
    }
    
    // Test 2: String concatenation length property
    bool test2_passed = rc::check("string concatenation length", [](const std::string& a, const std::string& b) {
        return (a + b).length() == a.length() + b.length();
    });
    
    if (test2_passed) {
        std::cout << "✓ Test 2: String property test passed" << std::endl;
    } else {
        std::cout << "✗ Test 2: String property test failed" << std::endl;
        return 1;
    }
    
    // Test 3: Vector size property
    bool test3_passed = rc::check("vector size after push_back", [](const std::vector<int>& vec, int elem) {
        std::vector<int> copy = vec;
        size_t original_size = copy.size();
        copy.push_back(elem);
        return copy.size() == original_size + 1;
    });
    
    if (test3_passed) {
        std::cout << "✓ Test 3: Vector property test passed" << std::endl;
    } else {
        std::cout << "✗ Test 3: Vector property test failed" << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "All RapidCheck setup tests passed!" << std::endl;
    std::cout << "RapidCheck is ready for OggDemuxer property-based testing." << std::endl;
    
    return 0;
    
#else
    std::cout << "RapidCheck is NOT available" << std::endl;
    std::cout << "To enable property-based testing, install librapidcheck-dev:" << std::endl;
    std::cout << "  sudo apt-get install librapidcheck-dev" << std::endl;
    std::cout << "Then reconfigure with: ./configure --enable-rapidcheck" << std::endl;
    
    // Return success even without RapidCheck - it's optional
    return 0;
#endif
}
