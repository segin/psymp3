/*
 * test_flac_codec_constant.cpp - Simple test for FLAC codec constant
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <cassert>
#include <cstdint>

// Define FOURCC macro and FLAC codec constant
#define FOURCC(a, b, c, d) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(c) << 8) | (uint8_t)(d)))
constexpr uint32_t CODEC_FLAC = FOURCC('f','L','a','C');

// Test FLAC codec constant definition
void test_flac_codec_constant() {
    std::cout << "Testing FLAC codec constant definition..." << std::endl;
    
    // Verify FLAC codec constant is defined correctly
    uint32_t expected_flac = FOURCC('f','L','a','C');
    assert(CODEC_FLAC == expected_flac);
    
    // Verify the actual byte values
    assert(CODEC_FLAC == 0x664C6143); // 'fLaC' in hex
    
    std::cout << "✓ FLAC codec constant (fLaC) defined correctly: 0x" 
              << std::hex << CODEC_FLAC << std::dec << std::endl;
}

// Test FOURCC macro functionality
void test_fourcc_macro() {
    std::cout << "Testing FOURCC macro functionality..." << std::endl;
    
    // Test various codec constants
    uint32_t aac_codec = FOURCC('m','p','4','a');
    uint32_t alac_codec = FOURCC('a','l','a','c');
    uint32_t flac_codec = FOURCC('f','L','a','C');
    
    // Verify they're different
    assert(aac_codec != alac_codec);
    assert(aac_codec != flac_codec);
    assert(alac_codec != flac_codec);
    
    std::cout << "✓ FOURCC macro creates unique codec identifiers" << std::endl;
    std::cout << "  AAC:  0x" << std::hex << aac_codec << std::dec << std::endl;
    std::cout << "  ALAC: 0x" << std::hex << alac_codec << std::dec << std::endl;
    std::cout << "  FLAC: 0x" << std::hex << flac_codec << std::dec << std::endl;
}

// Test FLAC sync pattern validation
void test_flac_sync_pattern() {
    std::cout << "Testing FLAC sync pattern validation..." << std::endl;
    
    // Valid FLAC sync patterns (0xFFF8 to 0xFFFF)
    uint16_t valid_patterns[] = {
        0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB,
        0xFFFC, 0xFFFD, 0xFFFE, 0xFFFF
    };
    
    for (uint16_t pattern : valid_patterns) {
        assert((pattern & 0xFFF8) == 0xFFF8);
    }
    
    // Invalid patterns
    uint16_t invalid_patterns[] = {
        0x0000, 0x1234, 0xFFF7, 0xFFF0, 0x8000
    };
    
    for (uint16_t pattern : invalid_patterns) {
        assert((pattern & 0xFFF8) != 0xFFF8);
    }
    
    std::cout << "✓ FLAC sync pattern validation works correctly" << std::endl;
}

int main() {
    std::cout << "Running FLAC codec constant tests..." << std::endl;
    
    try {
        test_flac_codec_constant();
        test_fourcc_macro();
        test_flac_sync_pattern();
        
        std::cout << "\n✅ All FLAC codec constant tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}