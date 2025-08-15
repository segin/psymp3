/*
 * generate_alaw_table.cpp - Generate ITU-T G.711 A-law lookup table
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdint>

// ITU-T G.711 A-law encoding/decoding functions
class ALawGenerator {
public:
    // Convert A-law encoded value to 16-bit linear PCM
    static int16_t alaw_to_linear(uint8_t alaw_val) {
        // A-law uses even-bit inversion (XOR with 0x55)
        alaw_val ^= 0x55;
        
        // Extract sign bit (bit 7)
        int sign = (alaw_val & 0x80) ? 1 : -1;
        
        // Extract exponent (bits 6-4)
        int exponent = (alaw_val & 0x70) >> 4;
        
        // Extract mantissa (bits 3-0)
        int mantissa = alaw_val & 0x0F;
        
        int linear;
        
        if (exponent == 0) {
            // Special case for exponent 0
            linear = (mantissa << 4) + 8;
        } else {
            // Normal case
            linear = ((mantissa << 4) + 0x108) << (exponent - 1);
        }
        
        return sign * linear;
    }
    
    // Generate the complete A-law to PCM lookup table
    static void generate_table() {
        std::cout << "// ITU-T G.711 A-law to 16-bit PCM conversion lookup table" << std::endl;
        std::cout << "const int16_t ALawCodec::ALAW_TO_PCM[256] = {" << std::endl;
        
        for (int i = 0; i < 256; i += 8) {
            std::cout << "    ";
            for (int j = 0; j < 8 && (i + j) < 256; ++j) {
                int16_t pcm_val = alaw_to_linear(i + j);
                std::cout << std::setw(6) << pcm_val;
                if (i + j < 255) {
                    std::cout << ",";
                }
                if (j < 7 && (i + j) < 255) {
                    std::cout << " ";
                }
            }
            std::cout << std::endl;
        }
        
        std::cout << "};" << std::endl;
    }
    
    // Test specific values
    static void test_values() {
        std::cout << "\nKey A-law test values:" << std::endl;
        std::cout << "======================" << std::endl;
        
        // Test silence values
        std::cout << "A-law 0x55 (silence after inversion) -> " << alaw_to_linear(0x55) << std::endl;
        std::cout << "A-law 0xD5 (ITU silence) -> " << alaw_to_linear(0xD5) << std::endl;
        std::cout << "A-law 0x00 -> " << alaw_to_linear(0x00) << std::endl;
        std::cout << "A-law 0x80 -> " << alaw_to_linear(0x80) << std::endl;
        std::cout << "A-law 0xFF -> " << alaw_to_linear(0xFF) << std::endl;
        
        // Find the actual silence value (closest to 0)
        std::cout << "\nSearching for silence value (PCM = 0):" << std::endl;
        int16_t min_abs = 32767;
        int silence_index = -1;
        
        for (int i = 0; i < 256; ++i) {
            int16_t pcm = alaw_to_linear(i);
            if (pcm == 0) {
                std::cout << "Found exact silence at A-law 0x" << std::hex << std::uppercase << i << std::dec << std::endl;
                silence_index = i;
                break;
            }
            if (abs(pcm) < min_abs) {
                min_abs = abs(pcm);
                silence_index = i;
            }
        }
        
        if (silence_index >= 0) {
            std::cout << "Closest to silence: A-law 0x" << std::hex << std::uppercase << silence_index 
                      << std::dec << " -> PCM " << alaw_to_linear(silence_index) << std::endl;
        }
    }
};

int main() {
    std::cout << "ITU-T G.711 A-law Lookup Table Generator" << std::endl;
    std::cout << "========================================" << std::endl;
    
    ALawGenerator::test_values();
    
    std::cout << "\nGenerating lookup table:" << std::endl;
    std::cout << "========================" << std::endl;
    
    ALawGenerator::generate_table();
    
    return 0;
}