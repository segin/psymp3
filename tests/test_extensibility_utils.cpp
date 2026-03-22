/*
 * test_extensibility_utils.cpp - Unit tests for Demuxer Extensibility utilities
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif

#include "demuxer/DemuxerExtensibility.h"
#include "test_framework.h"
#include <map>
#include <string>

using namespace PsyMP3::Demuxer::ExtensibilityUtils;
using namespace TestFramework;

class BuildURITest : public TestCase {
public:
    BuildURITest() : TestCase("ExtensibilityUtils::buildURIWithParameters") {}

protected:
    void runTest() override {
        std::map<std::string, std::string> params;

        // Test empty parameters
        ASSERT_EQUALS("http://example.com", buildURIWithParameters("http://example.com", params), "Empty params should return base URI");

        // Test single parameter
        params["key"] = "value";
        ASSERT_EQUALS("http://example.com?key=value", buildURIWithParameters("http://example.com", params), "Single param with no query in base");

        // Test multiple parameters (map is sorted by key)
        params["a"] = "1";
        ASSERT_EQUALS("http://example.com?a=1&key=value", buildURIWithParameters("http://example.com", params), "Multiple params with no query in base");

        // Test with existing query
        ASSERT_EQUALS("http://example.com?existing=true&a=1&key=value", buildURIWithParameters("http://example.com?existing=true", params), "Append to existing query");

        // Test with base URI ending in ?
        ASSERT_EQUALS("http://example.com?a=1&key=value", buildURIWithParameters("http://example.com?", params), "Base URI ending in ?");

        // Test with base URI ending in &
        ASSERT_EQUALS("http://example.com?foo=bar&a=1&key=value", buildURIWithParameters("http://example.com?foo=bar&", params), "Base URI ending in &");
    }
};

class ExtractParamsTest : public TestCase {
public:
    ExtractParamsTest() : TestCase("ExtensibilityUtils::extractURIParameters") {}

protected:
    void runTest() override {
        // Test no parameters
        auto p1 = extractURIParameters("http://example.com");
        ASSERT_TRUE(p1.empty(), "No query should return empty map");

        // Test one parameter
        auto p2 = extractURIParameters("http://example.com?key=value");
        ASSERT_EQUALS(1, p2.size(), "Should have one parameter");
        ASSERT_EQUALS("value", p2["key"], "Parameter value mismatch");

        // Test multiple parameters
        auto p3 = extractURIParameters("http://example.com?a=1&b=2&c=3");
        ASSERT_EQUALS(3, p3.size(), "Should extract all 3 parameters");
        ASSERT_EQUALS("1", p3["a"], "a mismatch");
        ASSERT_EQUALS("2", p3["b"], "b mismatch");
        ASSERT_EQUALS("3", p3["c"], "c mismatch");
    }
};

class FormatConfigStringTest : public TestCase {
public:
    FormatConfigStringTest() : TestCase("ExtensibilityUtils::formatConfigString") {}

protected:
    void runTest() override {
        // Empty
        std::map<std::string, std::string> empty_map;
        ASSERT_EQUALS("", formatConfigString(empty_map), "Empty map should return empty string");

        // Single
        std::map<std::string, std::string> single_map = {{"key1", "value1"}};
        ASSERT_EQUALS("key1=value1", formatConfigString(single_map), "Single pair map formatting failed");

        // Multiple
        std::map<std::string, std::string> multi_map = {
            {"key1", "value1"},
            {"key2", "value2"},
            {"key3", "value3"}
        };
        ASSERT_EQUALS("key1=value1;key2=value2;key3=value3", formatConfigString(multi_map), "Multiple pair map formatting failed");

        // Special chars
        std::map<std::string, std::string> special_map = {
            {"key with spaces", "value with spaces"},
            {"key=with=equals", "value;with;semicolons"},
            {"", "empty_key"}
        };
        ASSERT_EQUALS("=empty_key;key with spaces=value with spaces;key=with=equals=value;with;semicolons", 
            formatConfigString(special_map), "Special characters map formatting failed");
    }
};

class ConfigStringRoundTripTest : public TestCase {
public:
    ConfigStringRoundTripTest() : TestCase("ExtensibilityUtils::ConfigString round-trip") {}

protected:
    void runTest() override {
        std::map<std::string, std::string> original;
        original["key1"] = "value1";
        original["key2"] = "  value2  ";
        original["key3"] = "value3";

        std::string formatted = formatConfigString(original);
        auto parsed = parseConfigString(formatted);

        ASSERT_EQUALS(original.size(), parsed.size(), "Size mismatch after round-trip");
        ASSERT_EQUALS("value1", parsed["key1"], "key1 mismatch");
        ASSERT_EQUALS("value2", parsed["key2"], "key2 mismatch (should be trimmed)");
        ASSERT_EQUALS("value3", parsed["key3"], "key3 mismatch");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("ExtensibilityUtils Unit Tests");
    suite.addTest(std::make_unique<BuildURITest>());
    suite.addTest(std::make_unique<ExtractParamsTest>());
    suite.addTest(std::make_unique<FormatConfigStringTest>());
    suite.addTest(std::make_unique<ConfigStringRoundTripTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
