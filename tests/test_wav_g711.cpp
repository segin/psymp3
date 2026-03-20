/*
 * test_wav_g711.cpp - Test G.711 conversion utilities used in WaveStream
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "core/utility/G711.h"

using namespace PsyMP3::Core::Utility::G711;

#define ASSERT_EQUALS(expected, actual, message) \
    if ((expected) != (actual)) { \
        printf("FAIL: %s - Expected: %d, Got: %d\n", message, (int)(expected), (int)(actual)); \
        exit(1); \
    }

void testMuLaw() {
    printf("Testing Mu-law...\n");
    // 0xFF is silence (0)
    ASSERT_EQUALS(0, ulaw2linear(0xFF), "μ-law 0xFF should be 0");

    // 0x7F is also silence (0)
    ASSERT_EQUALS(0, ulaw2linear(0x7F), "μ-law 0x7F should be 0");

    // 0x00 is max negative
    ASSERT_EQUALS(-32124, ulaw2linear(0x00), "μ-law 0x00 should be -32124");

    // 0x80 is max positive
    ASSERT_EQUALS(32124, ulaw2linear(0x80), "μ-law 0x80 should be 32124");

    // Mid values
    ASSERT_EQUALS(16764, ulaw2linear(0x8F), "μ-law 0x8F should be 16764");
    ASSERT_EQUALS(-16764, ulaw2linear(0x0F), "μ-law 0x0F should be -16764");

    printf("Mu-law tests passed.\n");
}

void testALaw() {
    printf("Testing A-law...\n");
    // 0x55 is silence
    ASSERT_EQUALS(0, alaw2linear(0x55), "A-law 0x55 should be 0");

    // 0xD5 is silence
    ASSERT_EQUALS(0, alaw2linear(0xD5), "A-law 0xD5 should be 0");

    // Max negative (0x00)
    ASSERT_EQUALS(-5376, alaw2linear(0x00), "A-law 0x00 should be -5376");

    // Max positive (0x80)
    ASSERT_EQUALS(5376, alaw2linear(0x80), "A-law 0x80 should be 5376");

    // Some other values
    ASSERT_EQUALS(-832, alaw2linear(0x7F), "A-law 0x7F should be -832");
    ASSERT_EQUALS(832, alaw2linear(0xFF), "A-law 0xFF should be 832");

    printf("A-law tests passed.\n");
}

int main() {
    printf("Starting G.711 Conversion Tests\n");
    testMuLaw();
    testALaw();
    printf("All G.711 tests passed!\n");
    return 0;
}
