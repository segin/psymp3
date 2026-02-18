/*
 * test_base64_unit.cpp - Unit tests for Base64
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
#include <vector>
#include <string>

using namespace PsyMP3::Core::Utility;
using namespace TestFramework;

// ============================================================================
// Base64 Encoding Tests
// ============================================================================

class Base64EncodingTest : public TestCase {
public:
    Base64EncodingTest() : TestCase("Base64::encode") {}

protected:
    void runTest() override {
        // RFC 4648 Test Vectors
        ASSERT_EQUALS("", Base64::encode({}), "Empty input");
        ASSERT_EQUALS("Zg==", Base64::encode({'f'}), "f -> Zg==");
        ASSERT_EQUALS("Zm8=", Base64::encode({'f', 'o'}), "fo -> Zm8=");
        ASSERT_EQUALS("Zm9v", Base64::encode({'f', 'o', 'o'}), "foo -> Zm9v");
        ASSERT_EQUALS("Zm9vYg==", Base64::encode({'f', 'o', 'o', 'b'}), "foob -> Zm9vYg==");
        ASSERT_EQUALS("Zm9vYmE=", Base64::encode({'f', 'o', 'o', 'b', 'a'}), "fooba -> Zm9vYmE=");
        ASSERT_EQUALS("Zm9vYmFy", Base64::encode({'f', 'o', 'o', 'b', 'a', 'r'}), "foobar -> Zm9vYmFy");

        // Binary data
        std::vector<uint8_t> binary = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
        // 000000 000000 000100 000010 111111 111111 111011 111101
        // A      A      E      C      /      /      7      9
        ASSERT_EQUALS("AAEC//79", Base64::encode(binary), "Binary data encoding");
    }
};

// ============================================================================
// Base64 Decoding Tests
// ============================================================================

class Base64DecodingTest : public TestCase {
public:
    Base64DecodingTest() : TestCase("Base64::decode") {}

protected:
    void runTest() override {
        // RFC 4648 Test Vectors
        verifyDecode("", {});
        verifyDecode("Zg==", {'f'});
        verifyDecode("Zm8=", {'f', 'o'});
        verifyDecode("Zm9v", {'f', 'o', 'o'});
        verifyDecode("Zm9vYg==", {'f', 'o', 'o', 'b'});
        verifyDecode("Zm9vYmE=", {'f', 'o', 'o', 'b', 'a'});
        verifyDecode("Zm9vYmFy", {'f', 'o', 'o', 'b', 'a', 'r'});

        // Whitespace handling (should be ignored)
        verifyDecode(" Zm 9v ", {'f', 'o', 'o'});

        // Binary data
        verifyDecode("AAEC//79", {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD});
    }

private:
    void verifyDecode(const std::string& input, const std::vector<uint8_t>& expected) {
        std::vector<uint8_t> result = Base64::decode(input);

        std::ostringstream msg;
        msg << "Decoding '" << input << "' expected " << expected.size() << " bytes, got " << result.size();
        ASSERT_EQUALS(expected.size(), result.size(), msg.str());

        for (size_t i = 0; i < expected.size(); ++i) {
            std::ostringstream byteMsg;
            byteMsg << "Byte " << i << " mismatch";
            ASSERT_EQUALS(expected[i], result[i], byteMsg.str());
        }
    }
};

// ============================================================================
// Base64 Round Trip Tests
// ============================================================================

class Base64RoundTripTest : public TestCase {
public:
    Base64RoundTripTest() : TestCase("Base64::RoundTrip") {}

protected:
    void runTest() override {
        // Test all byte values
        std::vector<uint8_t> all_bytes;
        for (int i = 0; i < 256; ++i) {
            all_bytes.push_back(static_cast<uint8_t>(i));
        }

        std::string encoded = Base64::encode(all_bytes);
        std::vector<uint8_t> decoded = Base64::decode(encoded);

        ASSERT_EQUALS(all_bytes.size(), decoded.size(), "Round trip size match");
        for (size_t i = 0; i < all_bytes.size(); ++i) {
            ASSERT_EQUALS(all_bytes[i], decoded[i], "Round trip content match");
        }
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("Base64 Unit Tests");

    suite.addTest(std::make_unique<Base64EncodingTest>());
    suite.addTest(std::make_unique<Base64DecodingTest>());
    suite.addTest(std::make_unique<Base64RoundTripTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
