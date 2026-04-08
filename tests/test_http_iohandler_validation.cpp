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
        // HTTPIOHandler initialized with an empty URL should fail validation
        HTTPIOHandler handler("");

        // Since URL is empty, validateNetworkOperation("initialize") should fail
        // and handler should not be successfully initialized.
        ASSERT_FALSE(handler.isInitialized(), "Handler with empty URL should not be initialized");

        // validateNetworkOperation sets m_error to EINVAL when URL is empty
        ASSERT_TRUE(handler.getLastError() != 0, "Handler with empty URL should report an error");
        ASSERT_EQUALS(EINVAL, handler.getLastError(), "Error should be EINVAL for empty URL");
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
