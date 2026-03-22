/*
 * test_flac_parse_coded_number.cpp - Test FLAC UTF-8-like coded number parsing
 * This file is part of PsyMP3.
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"

#ifdef HAVE_FLAC

#include "codecs/flac/FLACRFC9639.h"
#include <cassert>
#include <iostream>
#include <vector>

void test_parse_1byte() {
    std::cout << "Testing 1-byte parsing..." << std::endl;
    uint8_t data[] = { 0x00 }; // 0
    uint64_t number = 0;
    size_t consumed = 0;

    assert(FLACRFC9639::parseCodedNumber(data, number, consumed) == true);
    assert(number == 0);
    assert(consumed == 1);

    uint8_t data2[] = { 0x7F }; // 127
    assert(FLACRFC9639::parseCodedNumber(data2, number, consumed) == true);
    assert(number == 127);
    assert(consumed == 1);

    std::cout << "✓ 1-byte coded numbers parsed correctly" << std::endl;
}

void test_parse_2byte() {
    std::cout << "Testing 2-byte parsing..." << std::endl;
    // 128 = 0x80 -> 11000010 10000000 = C2 80
    uint8_t data[] = { 0xC2, 0x80 };
    uint64_t number = 0;
    size_t consumed = 0;

    assert(FLACRFC9639::parseCodedNumber(data, number, consumed) == true);
    assert(number == 128);
    assert(consumed == 2);

    // 2047 = 0x7FF -> 11011111 10111111 = DF BF
    uint8_t data2[] = { 0xDF, 0xBF };
    assert(FLACRFC9639::parseCodedNumber(data2, number, consumed) == true);
    assert(number == 2047);
    assert(consumed == 2);

    std::cout << "✓ 2-byte coded numbers parsed correctly" << std::endl;
}

void test_parse_3byte() {
    std::cout << "Testing 3-byte parsing..." << std::endl;
    // 2048 = 0x800 -> 11100000 10100000 10000000 = E0 A0 80
    uint8_t data[] = { 0xE0, 0xA0, 0x80 };
    uint64_t number = 0;
    size_t consumed = 0;

    assert(FLACRFC9639::parseCodedNumber(data, number, consumed) == true);
    assert(number == 2048);
    assert(consumed == 3);

    // 65535 = 0xFFFF -> 11101111 10111111 10111111 = EF BF BF
    uint8_t data2[] = { 0xEF, 0xBF, 0xBF };
    assert(FLACRFC9639::parseCodedNumber(data2, number, consumed) == true);
    assert(number == 65535);
    assert(consumed == 3);

    std::cout << "✓ 3-byte coded numbers parsed correctly" << std::endl;
}

void test_parse_4byte() {
    std::cout << "Testing 4-byte parsing..." << std::endl;
    // 65536 = 0x10000 -> 11110000 10010000 10000000 10000000 = F0 90 80 80
    uint8_t data[] = { 0xF0, 0x90, 0x80, 0x80 };
    uint64_t number = 0;
    size_t consumed = 0;

    assert(FLACRFC9639::parseCodedNumber(data, number, consumed) == true);
    assert(number == 65536);
    assert(consumed == 4);

    std::cout << "✓ 4-byte coded numbers parsed correctly" << std::endl;
}

void test_parse_5byte() {
    std::cout << "Testing 5-byte parsing..." << std::endl;
    // Maximum 5-byte representation: 2^26 - 1 -> 11111011 10111111 10111111 10111111 10111111 = FB BF BF BF BF
    uint8_t data[] = { 0xFB, 0xBF, 0xBF, 0xBF, 0xBF };
    uint64_t number = 0;
    size_t consumed = 0;

    assert(FLACRFC9639::parseCodedNumber(data, number, consumed) == true);
    assert(number == ((1ULL << 26) - 1));
    assert(consumed == 5);

    std::cout << "✓ 5-byte coded numbers parsed correctly" << std::endl;
}

void test_parse_6byte() {
    std::cout << "Testing 6-byte parsing..." << std::endl;
    // Maximum 6-byte representation: 2^31 - 1 -> 11111101 10111111 10111111 10111111 10111111 10111111 = FD BF BF BF BF BF
    uint8_t data[] = { 0xFD, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF };
    uint64_t number = 0;
    size_t consumed = 0;

    assert(FLACRFC9639::parseCodedNumber(data, number, consumed) == true);
    assert(number == ((1ULL << 31) - 1));
    assert(consumed == 6);

    std::cout << "✓ 6-byte coded numbers parsed correctly" << std::endl;
}

void test_parse_7byte() {
    std::cout << "Testing 7-byte parsing..." << std::endl;
    // Maximum 7-byte representation: 2^36 - 1 -> 11111110 10111111 10111111 10111111 10111111 10111111 10111111 = FE BF BF BF BF BF BF
    uint8_t data[] = { 0xFE, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF };
    uint64_t number = 0;
    size_t consumed = 0;

    assert(FLACRFC9639::parseCodedNumber(data, number, consumed) == true);
    assert(number == ((1ULL << 36) - 1));
    assert(consumed == 7);

    std::cout << "✓ 7-byte coded numbers parsed correctly" << std::endl;
}

void test_parse_invalid() {
    std::cout << "Testing invalid inputs..." << std::endl;
    uint64_t number = 0;
    size_t consumed = 0;

    // Null pointer
    assert(FLACRFC9639::parseCodedNumber(nullptr, number, consumed) == false);

    // Invalid first byte: 0xFF
    uint8_t data_ff[] = { 0xFF };
    assert(FLACRFC9639::parseCodedNumber(data_ff, number, consumed) == false);

    // Invalid continuation byte (missing 0x80)
    uint8_t data_invalid_cont[] = { 0xC2, 0x00 };
    assert(FLACRFC9639::parseCodedNumber(data_invalid_cont, number, consumed) == false);

    // 3 bytes, second continuation invalid
    uint8_t data_invalid_cont2[] = { 0xE0, 0x80, 0x00 };
    assert(FLACRFC9639::parseCodedNumber(data_invalid_cont2, number, consumed) == false);

    std::cout << "✓ Invalid inputs correctly handled" << std::endl;
}

void test_parse_with_logging() {
    std::cout << "Testing FLACRFC9639::parseCodedNumberWithLogging..." << std::endl;
    uint8_t data[] = { 0x00 }; // 0
    uint64_t number = 0;
    size_t consumed = 0;

    // Test success case
    assert(FLACRFC9639::parseCodedNumberWithLogging(data, number, consumed) == true);
    assert(number == 0);
    assert(consumed == 1);

    // Test failure case
    uint8_t invalid_data[] = { 0xFF };
    assert(FLACRFC9639::parseCodedNumberWithLogging(invalid_data, number, consumed) == false);

    std::cout << "✓ Logging wrapper behaves correctly" << std::endl;
}

int main() {
    std::cout << "Running FLAC FLACRFC9639::parseCodedNumber Tests" << std::endl;
    std::cout << "===================================" << std::endl;

    try {
        test_parse_1byte();
        test_parse_2byte();
        test_parse_3byte();
        test_parse_4byte();
        test_parse_5byte();
        test_parse_6byte();
        test_parse_7byte();
        test_parse_invalid();
        test_parse_with_logging();

        std::cout << "\n✅ All FLACRFC9639::parseCodedNumber tests passed!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}

#else

int main() {
    std::cout << "FLAC support not available - skipping FLACRFC9639::parseCodedNumber tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC
