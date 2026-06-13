/*
 * test_http_iohandler_validation.cpp - Unit tests for HTTPIOHandler validation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/http/HTTPIOHandler.h"

using namespace PsyMP3::IO::HTTP;

class HTTPIOHandlerValidationTest : public TestFramework::TestCase {
public:
    HTTPIOHandlerValidationTest() : TestCase("HTTPIOHandler Network Validation") {}

protected:
    void runTest() override {
        // An empty URL fails validateNetworkOperation("initialize"), so the
        // constructor's initialize() throws rather than handing back a
        // half-built handler whose reads silently yield 0 bytes.
        bool threw = false;
        try {
            HTTPIOHandler handler("");
            (void)handler;
        } catch (const std::exception&) {
            threw = true;
        }
        ASSERT_TRUE(threw, "Constructing HTTPIOHandler with empty URL should throw");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestFramework::TestSuite suite("HTTPIOHandler Validation Tests");
    suite.addTest(std::make_unique<HTTPIOHandlerValidationTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
