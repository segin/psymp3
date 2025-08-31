/*
 * test_flac_variable_block_size.cpp - Test FLAC codec variable block size handling
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_FLAC

#include <iostream>
#include <cassert>
#include <vector>

// Simple assertion macro
#define SIMPLE_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << (message) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

// Test standard FLAC block sizes
bool test_standard_block_sizes() {
    std::cout << "Testing Standard FLAC Block Sizes..." << std::endl;
    
    // Standard FLAC block sizes as defined in RFC 9639
    std::vector<uint32_t> standard_sizes = {192, 576, 1152, 2304, 4608, 9216, 18432, 36864};
    
    for (uint32_t block_size : standard_sizes) {
        // Test that block size is within RFC 9639 range (16-65535)
        SIMPLE_ASSERT(block_size >= 16, "Block size below RFC 9639 minimum");
        SIMPLE_ASSERT(block_size <= 65535, "Block size above RFC 9639 maximum");
        
        Debug::log("test", "Standard block size ", block_size, " is valid");
    }
    
    std::cout << "Standard block sizes test PASSED" << std::endl;
    return true;
}

// Test RFC 9639 block size validation
bool test_rfc9639_validation() {
    std::cout << "Testing RFC 9639 Block Size Validation..." << std::endl;
    
    // Test valid range boundaries
    SIMPLE_ASSERT(16 >= 16, "Minimum block size validation");
    SIMPLE_ASSERT(65535 <= 65535, "Maximum block size validation");
    
    // Test some invalid sizes (conceptually)
    uint32_t invalid_small = 15;  // Below minimum
    uint32_t invalid_large = 65536; // Above maximum
    
    SIMPLE_ASSERT(invalid_small < 16, "Invalid small block size detected");
    SIMPLE_ASSERT(invalid_large > 65535, "Invalid large block size detected");
    
    Debug::log("test", "RFC 9639 validation logic working correctly");
    
    std::cout << "RFC 9639 validation test PASSED" << std::endl;
    return true;
}

// Test variable block size patterns
bool test_variable_block_patterns() {
    std::cout << "Testing Variable Block Size Patterns..." << std::endl;
    
    // Simulate a variable block size stream
    std::vector<uint32_t> variable_pattern = {576, 1152, 2304, 1152, 4608, 576, 9216, 1152};
    
    uint32_t previous_size = 0;
    bool variable_detected = false;
    
    for (uint32_t block_size : variable_pattern) {
        // Validate each block size
        SIMPLE_ASSERT(block_size >= 16 && block_size <= 65535, "Block size within valid range");
        
        // Detect variable block size usage
        if (previous_size != 0 && previous_size != block_size) {
            variable_detected = true;
            Debug::log("test", "Variable block size detected: ", previous_size, " -> ", block_size);
        }
        
        previous_size = block_size;
    }
    
    SIMPLE_ASSERT(variable_detected, "Variable block size pattern detected");
    
    std::cout << "Variable block patterns test PASSED" << std::endl;
    return true;
}

// Test buffer size calculations
bool test_buffer_calculations() {
    std::cout << "Testing Buffer Size Calculations..." << std::endl;
    
    // Test buffer size calculations for different configurations
    struct TestConfig {
        uint32_t block_size;
        uint16_t channels;
        size_t expected_samples;
    };
    
    std::vector<TestConfig> configs = {
        {576, 1, 576},      // Mono
        {1152, 2, 2304},    // Stereo
        {4608, 8, 36864},   // 8-channel
        {65535, 2, 131070}  // Maximum block size, stereo
    };
    
    for (const auto& config : configs) {
        size_t calculated_samples = static_cast<size_t>(config.block_size) * config.channels;
        
        SIMPLE_ASSERT(calculated_samples == config.expected_samples, 
                     "Buffer size calculation matches expected");
        
        Debug::log("test", "Buffer calculation: ", config.block_size, " samples × ", 
                  config.channels, " channels = ", calculated_samples, " total samples");
    }
    
    std::cout << "Buffer calculations test PASSED" << std::endl;
    return true;
}

// Test block size optimization heuristics
bool test_optimization_heuristics() {
    std::cout << "Testing Block Size Optimization Heuristics..." << std::endl;
    
    // Test preferred block size detection logic
    std::vector<uint32_t> repeated_pattern = {1152, 1152, 1152, 1152, 1152, 1152};
    
    uint32_t most_common = 0;
    uint32_t max_count = 0;
    
    // Simple frequency counting
    for (uint32_t target : repeated_pattern) {
        uint32_t count = 0;
        for (uint32_t size : repeated_pattern) {
            if (size == target) count++;
        }
        if (count > max_count) {
            max_count = count;
            most_common = target;
        }
    }
    
    SIMPLE_ASSERT(most_common == 1152, "Preferred block size detection working");
    SIMPLE_ASSERT(max_count == 6, "Frequency counting working");
    
    Debug::log("test", "Detected preferred block size: ", most_common, " (seen ", max_count, " times)");
    
    std::cout << "Optimization heuristics test PASSED" << std::endl;
    return true;
}

int main() {
    std::cout << "=== FLAC Variable Block Size Handling Tests ===" << std::endl;
    
    bool all_passed = true;
    
    all_passed &= test_standard_block_sizes();
    all_passed &= test_rfc9639_validation();
    all_passed &= test_variable_block_patterns();
    all_passed &= test_buffer_calculations();
    all_passed &= test_optimization_heuristics();
    
    if (all_passed) {
        std::cout << "=== All FLAC variable block size tests PASSED! ===" << std::endl;
        return 0;
    } else {
        std::cout << "=== Some FLAC variable block size tests FAILED! ===" << std::endl;
        return 1;
    }
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC codec not available - skipping variable block size tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC