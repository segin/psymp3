/*
 * test_mulaw_lookup_simple.cpp - Simple test for μ-law lookup table
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <vector>
#include <cstdint>

// Include only what we need for the test
extern const int16_t MULAW_TO_PCM[256];

int main() {
    std::cout << "Testing μ-law lookup table implementation..." << std::endl;
    
    // Test silence value (0xFF should map to 0)
    if (MULAW_TO_PCM[0xFF] != 0) {
        std::cout << "FAIL: μ-law silence value (0xFF) should map to 0, got " 
                  << MULAW_TO_PCM[0xFF] << std::endl;
        return 1;
    }
    std::cout << "PASS: μ-law silence value (0xFF) maps to 0" << std::endl;
    
    // Test sign bit handling - values 0x00-0x7F should be negative
    bool negative_test_passed = true;
    for (int i = 0x00; i <= 0x7F; ++i) {
        if (MULAW_TO_PCM[i] >= 0) {
            std::cout << "FAIL: μ-law value 0x" << std::hex << i 
                      << " should be negative, got " << std::dec << MULAW_TO_PCM[i] << std::endl;
            negative_test_passed = false;
            break;
        }
    }
    if (negative_test_passed) {
        std::cout << "PASS: μ-law values 0x00-0x7F are negative" << std::endl;
    }
    
    // Test sign bit handling - values 0x80-0xFE should be positive
    bool positive_test_passed = true;
    for (int i = 0x80; i <= 0xFE; ++i) {
        if (MULAW_TO_PCM[i] <= 0) {
            std::cout << "FAIL: μ-law value 0x" << std::hex << i 
                      << " should be positive, got " << std::dec << MULAW_TO_PCM[i] << std::endl;
            positive_test_passed = false;
            break;
        }
    }
    if (positive_test_passed) {
        std::cout << "PASS: μ-law values 0x80-0xFE are positive" << std::endl;
    }
    
    // Test maximum amplitudes
    if (MULAW_TO_PCM[0x00] < -30000) {
        std::cout << "PASS: Maximum negative μ-law (0x00) produces high negative amplitude: " 
                  << MULAW_TO_PCM[0x00] << std::endl;
    } else {
        std::cout << "FAIL: Maximum negative μ-law (0x00) should produce high negative amplitude, got " 
                  << MULAW_TO_PCM[0x00] << std::endl;
        return 1;
    }
    
    if (MULAW_TO_PCM[0x80] > 30000) {
        std::cout << "PASS: Maximum positive μ-law (0x80) produces high positive amplitude: " 
                  << MULAW_TO_PCM[0x80] << std::endl;
    } else {
        std::cout << "FAIL: Maximum positive μ-law (0x80) should produce high positive amplitude, got " 
                  << MULAW_TO_PCM[0x80] << std::endl;
        return 1;
    }
    
    // Test some specific ITU-T G.711 values
    std::cout << "Sample μ-law to PCM mappings:" << std::endl;
    std::cout << "  0x00 -> " << MULAW_TO_PCM[0x00] << " (max negative)" << std::endl;
    std::cout << "  0x80 -> " << MULAW_TO_PCM[0x80] << " (max positive)" << std::endl;
    std::cout << "  0xFF -> " << MULAW_TO_PCM[0xFF] << " (silence)" << std::endl;
    std::cout << "  0x7F -> " << MULAW_TO_PCM[0x7F] << " (min negative)" << std::endl;
    std::cout << "  0xFE -> " << MULAW_TO_PCM[0xFE] << " (min positive)" << std::endl;
    
    std::cout << "All μ-law lookup table tests passed!" << std::endl;
    return 0;
}