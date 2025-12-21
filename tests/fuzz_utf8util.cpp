/*
 * fuzz_utf8util.cpp - Fuzzing tests for UTF8Util
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * This fuzzer tests UTF8Util with random/malformed input to ensure
 * robustness against malicious or corrupted data.
 */

#include "psymp3.h"
#include <iostream>
#include <random>
#include <chrono>
#include <cassert>

using namespace PsyMP3::Core::Utility;

// ============================================================================
// Random Data Generator
// ============================================================================

class RandomGenerator {
public:
    RandomGenerator(uint64_t seed = 0) {
        if (seed == 0) {
            seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        }
        m_rng.seed(seed);
        m_seed = seed;
    }
    
    uint64_t seed() const { return m_seed; }
    
    uint8_t randomByte() {
        return static_cast<uint8_t>(m_dist(m_rng));
    }
    
    std::vector<uint8_t> randomBytes(size_t count) {
        std::vector<uint8_t> result(count);
        for (size_t i = 0; i < count; ++i) {
            result[i] = randomByte();
        }
        return result;
    }
    
    std::string randomString(size_t maxLen) {
        size_t len = m_dist(m_rng) % (maxLen + 1);
        std::string result(len, '\0');
        for (size_t i = 0; i < len; ++i) {
            result[i] = static_cast<char>(randomByte());
        }
        return result;
    }
    
    // Generate valid UTF-8 string (avoiding null characters which terminate strings)
    std::string randomValidUTF8(size_t maxCodepoints) {
        size_t count = m_dist(m_rng) % (maxCodepoints + 1);
        std::string result;
        for (size_t i = 0; i < count; ++i) {
            uint32_t cp;
            do {
                cp = m_dist(m_rng) % 0x110000;
            } while (!UTF8Util::isValidCodepoint(cp) || cp == 0); // Avoid null
            result += UTF8Util::encodeCodepoint(cp);
        }
        return result;
    }
    
private:
    std::mt19937_64 m_rng;
    std::uniform_int_distribution<uint32_t> m_dist{0, 255};
    uint64_t m_seed;
};

// ============================================================================
// Fuzz Test: isValid() doesn't crash on random input
// ============================================================================

void fuzz_isValid(RandomGenerator& rng, int iterations) {
    std::cout << "Fuzzing UTF8Util::isValid()..." << std::endl;
    
    for (int i = 0; i < iterations; ++i) {
        std::string input = rng.randomString(1000);
        // Should not crash, just return true or false
        bool result = UTF8Util::isValid(input);
        (void)result; // Suppress unused warning
    }
    
    std::cout << "  ✓ " << iterations << " iterations completed without crash" << std::endl;
}

// ============================================================================
// Fuzz Test: repair() always produces valid UTF-8
// ============================================================================

void fuzz_repair(RandomGenerator& rng, int iterations) {
    std::cout << "Fuzzing UTF8Util::repair()..." << std::endl;
    
    int valid_count = 0;
    for (int i = 0; i < iterations; ++i) {
        std::string input = rng.randomString(500);
        std::string repaired = UTF8Util::repair(input);
        
        if (UTF8Util::isValid(repaired)) {
            valid_count++;
        } else {
            std::cerr << "  FAIL: repair() produced invalid UTF-8 at iteration " << i << std::endl;
        }
    }
    
    assert(valid_count == iterations && "All repaired strings must be valid UTF-8");
    std::cout << "  ✓ " << iterations << " iterations - all outputs valid UTF-8" << std::endl;
}

// ============================================================================
// Fuzz Test: fromLatin1() doesn't crash
// ============================================================================

void fuzz_fromLatin1(RandomGenerator& rng, int iterations) {
    std::cout << "Fuzzing UTF8Util::fromLatin1()..." << std::endl;
    
    for (int i = 0; i < iterations; ++i) {
        auto input = rng.randomBytes(500);
        std::string result = UTF8Util::fromLatin1(input.data(), input.size());
        
        // Result should always be valid UTF-8
        assert(UTF8Util::isValid(result) && "fromLatin1 must produce valid UTF-8");
    }
    
    std::cout << "  ✓ " << iterations << " iterations - all outputs valid UTF-8" << std::endl;
}

// ============================================================================
// Fuzz Test: fromUTF16LE() doesn't crash on random input
// ============================================================================

void fuzz_fromUTF16LE(RandomGenerator& rng, int iterations) {
    std::cout << "Fuzzing UTF8Util::fromUTF16LE()..." << std::endl;
    
    for (int i = 0; i < iterations; ++i) {
        auto input = rng.randomBytes(500);
        std::string result = UTF8Util::fromUTF16LE(input.data(), input.size());
        
        // Result should always be valid UTF-8 (with replacement chars for invalid)
        assert(UTF8Util::isValid(result) && "fromUTF16LE must produce valid UTF-8");
    }
    
    std::cout << "  ✓ " << iterations << " iterations - all outputs valid UTF-8" << std::endl;
}

// ============================================================================
// Fuzz Test: fromUTF16BE() doesn't crash on random input
// ============================================================================

void fuzz_fromUTF16BE(RandomGenerator& rng, int iterations) {
    std::cout << "Fuzzing UTF8Util::fromUTF16BE()..." << std::endl;
    
    for (int i = 0; i < iterations; ++i) {
        auto input = rng.randomBytes(500);
        std::string result = UTF8Util::fromUTF16BE(input.data(), input.size());
        
        assert(UTF8Util::isValid(result) && "fromUTF16BE must produce valid UTF-8");
    }
    
    std::cout << "  ✓ " << iterations << " iterations - all outputs valid UTF-8" << std::endl;
}

// ============================================================================
// Fuzz Test: fromUTF16BOM() doesn't crash on random input
// ============================================================================

void fuzz_fromUTF16BOM(RandomGenerator& rng, int iterations) {
    std::cout << "Fuzzing UTF8Util::fromUTF16BOM()..." << std::endl;
    
    for (int i = 0; i < iterations; ++i) {
        auto input = rng.randomBytes(500);
        std::string result = UTF8Util::fromUTF16BOM(input.data(), input.size());
        
        assert(UTF8Util::isValid(result) && "fromUTF16BOM must produce valid UTF-8");
    }
    
    std::cout << "  ✓ " << iterations << " iterations - all outputs valid UTF-8" << std::endl;
}

// ============================================================================
// Fuzz Test: fromUTF32LE() doesn't crash on random input
// ============================================================================

void fuzz_fromUTF32LE(RandomGenerator& rng, int iterations) {
    std::cout << "Fuzzing UTF8Util::fromUTF32LE()..." << std::endl;
    
    for (int i = 0; i < iterations; ++i) {
        auto input = rng.randomBytes(500);
        std::string result = UTF8Util::fromUTF32LE(input.data(), input.size());
        
        assert(UTF8Util::isValid(result) && "fromUTF32LE must produce valid UTF-8");
    }
    
    std::cout << "  ✓ " << iterations << " iterations - all outputs valid UTF-8" << std::endl;
}

// ============================================================================
// Fuzz Test: decodeCodepoint() doesn't crash on random input
// ============================================================================

void fuzz_decodeCodepoint(RandomGenerator& rng, int iterations) {
    std::cout << "Fuzzing UTF8Util::decodeCodepoint()..." << std::endl;
    
    for (int i = 0; i < iterations; ++i) {
        auto input = rng.randomBytes(10);
        size_t consumed;
        uint32_t cp = UTF8Util::decodeCodepoint(input.data(), input.size(), consumed);
        
        // Should return a valid codepoint or replacement char
        assert((UTF8Util::isValidCodepoint(cp) || cp == 0xFFFD) && 
               "decodeCodepoint must return valid codepoint or U+FFFD");
        assert(consumed > 0 && consumed <= input.size() && 
               "consumed must be positive and within bounds");
    }
    
    std::cout << "  ✓ " << iterations << " iterations completed" << std::endl;
}

// ============================================================================
// Fuzz Test: toCodepoints() doesn't crash on random input
// ============================================================================

void fuzz_toCodepoints(RandomGenerator& rng, int iterations) {
    std::cout << "Fuzzing UTF8Util::toCodepoints()..." << std::endl;
    
    for (int i = 0; i < iterations; ++i) {
        std::string input = rng.randomString(200);
        auto codepoints = UTF8Util::toCodepoints(input);
        
        // All codepoints should be valid or replacement char
        for (uint32_t cp : codepoints) {
            assert((UTF8Util::isValidCodepoint(cp) || cp == 0xFFFD) && 
                   "All codepoints must be valid or U+FFFD");
        }
    }
    
    std::cout << "  ✓ " << iterations << " iterations completed" << std::endl;
}

// ============================================================================
// Fuzz Test: Round-trip with valid UTF-8
// ============================================================================

void fuzz_roundtrip(RandomGenerator& rng, int iterations) {
    std::cout << "Fuzzing round-trip with valid UTF-8..." << std::endl;
    
    int failures = 0;
    for (int i = 0; i < iterations; ++i) {
        std::string original = rng.randomValidUTF8(100);
        
        // UTF-16LE round-trip
        auto utf16le = UTF8Util::toUTF16LE(original);
        std::string decoded_le = UTF8Util::fromUTF16LE(utf16le.data(), utf16le.size());
        if (decoded_le != original) failures++;
        
        // UTF-16BE round-trip
        auto utf16be = UTF8Util::toUTF16BE(original);
        std::string decoded_be = UTF8Util::fromUTF16BE(utf16be.data(), utf16be.size());
        if (decoded_be != original) failures++;
        
        // UTF-32 round-trip
        auto utf32 = UTF8Util::toUTF32LE(original);
        std::string decoded_32 = UTF8Util::fromUTF32LE(utf32.data(), utf32.size());
        if (decoded_32 != original) failures++;
    }
    
    assert(failures == 0 && "All round-trips must preserve data");
    std::cout << "  ✓ " << iterations << " iterations - all round-trips successful" << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    int iterations = 1000;
    uint64_t seed = 0;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-n" && i + 1 < argc) {
            iterations = std::stoi(argv[++i]);
        } else if (arg == "-s" && i + 1 < argc) {
            seed = std::stoull(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [-n iterations] [-s seed]" << std::endl;
            return 0;
        }
    }
    
    RandomGenerator rng(seed);
    
    std::cout << "========================================" << std::endl;
    std::cout << "UTF8Util Fuzzing Tests" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Seed: " << rng.seed() << std::endl;
    std::cout << "========================================" << std::endl;
    
    fuzz_isValid(rng, iterations);
    fuzz_repair(rng, iterations);
    fuzz_fromLatin1(rng, iterations);
    fuzz_fromUTF16LE(rng, iterations);
    fuzz_fromUTF16BE(rng, iterations);
    fuzz_fromUTF16BOM(rng, iterations);
    fuzz_fromUTF32LE(rng, iterations);
    fuzz_decodeCodepoint(rng, iterations);
    fuzz_toCodepoints(rng, iterations);
    fuzz_roundtrip(rng, iterations);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All fuzzing tests PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}

