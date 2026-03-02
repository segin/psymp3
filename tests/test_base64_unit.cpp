/*
 * test_base64_unit.cpp - Unit tests for Base64 utility
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif
#include "test_framework.h"
#include "core/utility/Base64.h"

#include <string>
#include <vector>
#include <memory>
#include <sstream>

using namespace PsyMP3::Core::Utility;
using namespace TestFramework;

// ============================================================================
// RFC 4648 Test Vectors
// ============================================================================

class Base64RFCTest : public TestCase {
public:
    Base64RFCTest() : TestCase("Base64 RFC 4648 Vectors") {}

protected:
    void runTest() override {
        // Test vectors from RFC 4648 section 10
        // BASE64("") = ""
        // BASE64("f") = "Zg=="
        // BASE64("fo") = "Zm8="
        // BASE64("foo") = "Zm9v"
        // BASE64("foob") = "Zm9vYg=="
        // BASE64("fooba") = "Zm9vYmE="
        // BASE64("foobar") = "Zm9vYmFy"

        checkEncode("", "");
        checkEncode("f", "Zg==");
        checkEncode("fo", "Zm8=");
        checkEncode("foo", "Zm9v");
        checkEncode("foob", "Zm9vYg==");
        checkEncode("fooba", "Zm9vYmE=");
        checkEncode("foobar", "Zm9vYmFy");

        checkDecode("", "");
        checkDecode("Zg==", "f");
        checkDecode("Zm8=", "fo");
        checkDecode("Zm9v", "foo");
        checkDecode("Zm9vYg==", "foob");
        checkDecode("Zm9vYmE=", "fooba");
        checkDecode("Zm9vYmFy", "foobar");
    }

private:
    void checkEncode(const std::string& input, const std::string& expected) {
        std::vector<uint8_t> data(input.begin(), input.end());
        ASSERT_EQUALS(expected, Base64::encode(data), "Encoding '" + input + "' failed");
    }

    void checkDecode(const std::string& input, const std::string& expected) {
        std::vector<uint8_t> decoded = Base64::decode(input);
        std::string result(decoded.begin(), decoded.end());
        ASSERT_EQUALS(expected, result, "Decoding '" + input + "' failed");
    }
};

// ============================================================================
// Base64 Encoding Tests (Additional Binary Data)
// ============================================================================

class Base64EncodingTest : public TestCase {
public:
    Base64EncodingTest() : TestCase("Base64::encode extra") {}

protected:
    void runTest() override {
        // Binary data
        std::vector<uint8_t> binary = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
        // 000000 000000 000100 000010 111111 111111 111011 111101
        // A      A      E      C      /      /      7      9
        ASSERT_EQUALS("AAEC//79", Base64::encode(binary), "Binary data encoding");
    }
};

// ============================================================================
// Base64 Round-trip Tests
// ============================================================================

class Base64RoundTripTest : public TestCase {
public:
    Base64RoundTripTest() : TestCase("Base64 Round-trip") {}

protected:
    void runTest() override {
        // Test with all byte values
        std::vector<uint8_t> data;
        for (int i = 0; i < 256; ++i) {
            data.push_back(static_cast<uint8_t>(i));
        }

        std::string encoded = Base64::encode(data);
        std::vector<uint8_t> decoded = Base64::decode(encoded);

        ASSERT_EQUALS(data.size(), decoded.size(), "Decoded size mismatch");
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] != decoded[i]) {
                ASSERT_TRUE(false, "Decoded data mismatch at index " + std::to_string(i));
            }
        }
    }
};

// ============================================================================
// Robustness Tests
// ============================================================================

class Base64RobustnessTest : public TestCase {
public:
    Base64RobustnessTest() : TestCase("Base64 Robustness") {}

protected:
    void runTest() override {
        // Whitespace handling
        std::string input = "Zm9v\nYmFy"; // "foobar" with newline
        std::vector<uint8_t> decoded = Base64::decode(input);
        std::string result(decoded.begin(), decoded.end());
        ASSERT_EQUALS("foobar", result, "Decoding with newline failed");

        input = "Zm9v YmFy"; // "foobar" with space
        decoded = Base64::decode(input);
        result.assign(decoded.begin(), decoded.end());
        ASSERT_EQUALS("foobar", result, "Decoding with space failed");

        // Invalid characters should be ignored
        input = "Zm9v?YmFy";
        decoded = Base64::decode(input);
        result.assign(decoded.begin(), decoded.end());
        ASSERT_EQUALS("foobar", result, "Decoding with invalid char failed");

        // Truncated input at padding
        input = "Zm9vYmE"; // Should be "Zm9vYmE="
        decoded = Base64::decode(input);
        result.assign(decoded.begin(), decoded.end());
        ASSERT_EQUALS("fooba", result, "Decoding truncated input failed");
    }
};

// ============================================================================
// Test Registration
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("Base64 Unit Tests");

    suite.addTest(std::make_unique<Base64RFCTest>());
    suite.addTest(std::make_unique<Base64EncodingTest>());
    suite.addTest(std::make_unique<Base64RoundTripTest>());
    suite.addTest(std::make_unique<Base64RobustnessTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
