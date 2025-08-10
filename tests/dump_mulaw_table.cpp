/*
 * dump_mulaw_table.cpp - Dump μ-law lookup table values for verification
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <iomanip>
#include <cstdint>

// We'll access the table by declaring it as extern
// This works because the table is defined with external linkage
extern "C" const int16_t _ZN10MuLawCodec12MULAW_TO_PCME[256];

int main() {
    std::cout << "μ-law to PCM Lookup Table Values:" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Test key values for ITU-T G.711 compliance
    const int16_t* table = _ZN10MuLawCodec12MULAW_TO_PCME;
    
    std::cout << "Key test values:" << std::endl;
    std::cout << "0x00 (max negative): " << table[0x00] << std::endl;
    std::cout << "0x80 (max positive): " << table[0x80] << std::endl;
    std::cout << "0xFF (silence):      " << table[0xFF] << std::endl;
    std::cout << "0x7F (min negative): " << table[0x7F] << std::endl;
    std::cout << "0xFE (min positive): " << table[0xFE] << std::endl;
    
    // Verify sign bit handling
    bool negative_ok = true, positive_ok = true;
    
    for (int i = 0x00; i <= 0x7F; ++i) {
        if (table[i] >= 0) {
            negative_ok = false;
            break;
        }
    }
    
    for (int i = 0x80; i <= 0xFE; ++i) {
        if (table[i] <= 0) {
            positive_ok = false;
            break;
        }
    }
    
    std::cout << std::endl;
    std::cout << "Validation Results:" << std::endl;
    std::cout << "==================" << std::endl;
    std::cout << "Silence value (0xFF = 0): " << (table[0xFF] == 0 ? "✓ PASS" : "✗ FAIL") << std::endl;
    std::cout << "Negative values (0x00-0x7F): " << (negative_ok ? "✓ PASS" : "✗ FAIL") << std::endl;
    std::cout << "Positive values (0x80-0xFE): " << (positive_ok ? "✓ PASS" : "✗ FAIL") << std::endl;
    std::cout << "Max negative amplitude: " << (table[0x00] < -30000 ? "✓ PASS" : "✗ FAIL") << std::endl;
    std::cout << "Max positive amplitude: " << (table[0x80] > 30000 ? "✓ PASS" : "✗ FAIL") << std::endl;
    
    if (table[0xFF] == 0 && negative_ok && positive_ok && 
        table[0x00] < -30000 && table[0x80] > 30000) {
        std::cout << std::endl << "✓ All ITU-T G.711 μ-law compliance tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << std::endl << "✗ Some compliance tests FAILED!" << std::endl;
        return 1;
    }
}