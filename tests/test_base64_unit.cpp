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
#include <vector>
#include <string>
#include <sstream>
#include <memory>

using namespace PsyMP3::Core::Utility;
using namespace TestFramework;

// Helper to check vector equality since ASSERT_EQUALS doesn't support vector printing
void assertVectorsEqual(const std::vector<uint8_t>& expected, const std::vector<uint8_t>& actual, const std::string& message) {
    if (expected.size() != actual.size()) {
        std::ostringstream oss;
        oss << message << " - Size mismatch. Expected: " << expected.size() << ", Got: " << actual.size();
        throw TestFramework::AssertionFailure(oss.str());
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] != actual[i]) {
            std::ostringstream oss;
            oss << message << " - Content mismatch at index " << i
                << ". Expected: " << (int)expected[i] << ", Got: " << (int)actual[i];
            throw TestFramework::AssertionFailure(oss.str());
        }
    }
}

class Base64EncodingTest : public TestCase {
public:
    Base64EncodingTest() : TestCase("Base64::encode") {}

protected:
    void runTest() override {
        // RFC 4648 test vectors
        ASSERT_EQUALS("", Base64::encode({}), "Empty input");
        ASSERT_EQUALS("Zg==", Base64::encode({'f'}), "f -> Zg==");
        ASSERT_EQUALS("Zm8=", Base64::encode({'f', 'o'}), "fo -> Zm8=");
        ASSERT_EQUALS("Zm9v", Base64::encode({'f', 'o', 'o'}), "foo -> Zm9v");
        ASSERT_EQUALS("Zm9vYg==", Base64::encode({'f', 'o', 'o', 'b'}), "foob -> Zm9vYg==");
        ASSERT_EQUALS("Zm9vYmE=", Base64::encode({'f', 'o', 'o', 'b', 'a'}), "fooba -> Zm9vYmE=");
        ASSERT_EQUALS("Zm9vYmFy", Base64::encode({'f', 'o', 'o', 'b', 'a', 'r'}), "foobar -> Zm9vYmFy");
    }
};

class Base64DecodingTest : public TestCase {
public:
    Base64DecodingTest() : TestCase("Base64::decode") {}

protected:
    void runTest() override {
        // RFC 4648 test vectors
        checkDecode("", {});
        checkDecode("Zg==", {'f'});
        checkDecode("Zm8=", {'f', 'o'});
        checkDecode("Zm9v", {'f', 'o', 'o'});
        checkDecode("Zm9vYg==", {'f', 'o', 'o', 'b'});
        checkDecode("Zm9vYmE=", {'f', 'o', 'o', 'b', 'a'});
        checkDecode("Zm9vYmFy", {'f', 'o', 'o', 'b', 'a', 'r'});

        // Whitespace handling (should be ignored)
        checkDecode("Z g = =", {'f'});
        checkDecode("Zm 9v", {'f', 'o', 'o'});
    }

private:
    void checkDecode(const std::string& input, const std::vector<uint8_t>& expected) {
        std::vector<uint8_t> result = Base64::decode(input);
        assertVectorsEqual(expected, result, "Decoding '" + input + "'");
    }
};

class Base64RoundTripTest : public TestCase {
public:
    Base64RoundTripTest() : TestCase("Base64::roundTrip") {}

protected:
    void runTest() override {
        std::vector<uint8_t> data;
        for (int i = 0; i < 256; ++i) {
            data.push_back(static_cast<uint8_t>(i));
        }

        std::string encoded = Base64::encode(data);
        std::vector<uint8_t> decoded = Base64::decode(encoded);

        assertVectorsEqual(data, decoded, "Round-trip with all byte values");
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
