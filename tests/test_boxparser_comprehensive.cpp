/*
 * test_boxparser_comprehensive.cpp - Comprehensive tests for BoxParser
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#else
#include <iostream>
#include <vector>
#include <cstring>
#include <memory>
#endif
#include "test_framework.h"
#include "demuxer/iso/BoxParser.h"
#include "demuxer/iso/ISODemuxer.h"
#include "io/MemoryIOHandler.h"
#include <vector>
#include <cstring>
#include <memory>
#include <iostream>

using namespace PsyMP3;
using namespace PsyMP3::Demuxer::ISO;
using namespace PsyMP3::IO;
using namespace TestFramework;

class BoxParserTest : public TestCase {
public:
    BoxParserTest() : TestCase("BoxParser Comprehensive Tests") {}

protected:
    void runTest() override {
        testReadBoxHeader();
        testValidateBoxSize();
        testParseFileTypeBox();
        testOOMProtection();
        testInvalidBoxHandling();
    }

private:
    void testReadBoxHeader() {
        // Prepare data for a valid box
        // Size: 8 bytes, Type: 'test'
        std::vector<uint8_t> data = {
            0x00, 0x00, 0x00, 0x08, // Size
            't', 'e', 's', 't'      // Type
        };

        auto io = std::make_shared<MemoryIOHandler>(data.data(), data.size());
        BoxParser parser(io);

        BoxHeader header = parser.ReadBoxHeader(0);
        ASSERT_EQUALS(8ULL, header.size, "Box size should be 8");
        ASSERT_EQUALS(0x74657374U, header.type, "Box type should be 'test'"); // 'test' in hex
        ASSERT_EQUALS(8ULL, header.dataOffset, "Data offset should be 8");
        ASSERT_FALSE(header.extendedSize, "Should not be extended size");
    }

    void testValidateBoxSize() {
        std::vector<uint8_t> data(100); // 100 bytes file
        auto io = std::make_shared<MemoryIOHandler>(data.data(), data.size());
        BoxParser parser(io);

        BoxHeader header;
        header.type = 0x74657374; // 'test'
        header.size = 20;
        header.dataOffset = 8;
        header.extendedSize = false;

        // Valid case
        ASSERT_TRUE(parser.ValidateBoxSize(header, 50), "Should be valid (fits in container)");

        // Invalid: larger than container
        header.size = 60;
        ASSERT_FALSE(parser.ValidateBoxSize(header, 50), "Should be invalid (larger than container)");

        // Invalid: larger than file
        header.size = 200;
        ASSERT_FALSE(parser.ValidateBoxSize(header, 500), "Should be invalid (larger than file)");

        // Invalid: zero size
        header.size = 0;
        ASSERT_FALSE(parser.ValidateBoxSize(header, 50), "Should be invalid (size 0)");
    }

    void testParseFileTypeBox() {
         std::vector<uint8_t> data = {
            'i', 's', 'o', 'm', // Major Brand
            0x00, 0x00, 0x00, 0x01, // Minor Version
            'm', 'p', '4', '1'  // Compatible Brands
        };

        auto io = std::make_shared<MemoryIOHandler>(data.data(), data.size());
        BoxParser parser(io);

        std::string containerType;
        bool result = parser.ParseFileTypeBox(0, 12, containerType);

        ASSERT_TRUE(result, "ParseFileTypeBox should succeed");
        ASSERT_EQUALS(std::string("MP4"), containerType, "Container type should be MP4");
    }

    void testOOMProtection() {
        // Construct a malicious 'stts' box in memory
        // 4 bytes size, 4 bytes type ('stts'), 1 byte version, 3 bytes flags
        // 4 bytes entry_count (set to huge value)

        std::vector<uint8_t> buffer;
        buffer.resize(100, 0);

        // Version = 0, Flags = 0
        buffer[0] = 0; buffer[1] = 0; buffer[2] = 0; buffer[3] = 0;

        // Entry Count = 1 (to pass basic check)
        buffer[4] = 0; buffer[5] = 0; buffer[6] = 0; buffer[7] = 1;

        // Entry 1: Sample Count (Huge!)
        // MAX_SAMPLES_PER_TRACK is 10000000
        // Set to 20000000
        uint32_t hugeCount = 20000000;
        buffer[8] = (hugeCount >> 24) & 0xFF;
        buffer[9] = (hugeCount >> 16) & 0xFF;
        buffer[10] = (hugeCount >> 8) & 0xFF;
        buffer[11] = hugeCount & 0xFF;

        // Entry 1: Sample Delta
        buffer[12] = 0; buffer[13] = 0; buffer[14] = 0; buffer[15] = 1;

        auto io = std::make_shared<MemoryIOHandler>(buffer.data(), buffer.size());
        BoxParser parser(io);

        SampleTableInfo tables;

        // Pass offset 0, which corresponds to where Version/Flags start in our buffer
        // because ParseTimeToSampleBox expects offset to point to box data (after header)
        bool result = parser.ParseTimeToSampleBox(0, 16, tables);

        ASSERT_FALSE(result, "Should reject stts with too many samples");
    }

    void testInvalidBoxHandling() {
         // Test read header beyond EOF
         std::vector<uint8_t> data = { 0x00 };
         auto io = std::make_shared<MemoryIOHandler>(data.data(), data.size());
         BoxParser parser(io);

         BoxHeader header = parser.ReadBoxHeader(10);
         ASSERT_EQUALS(0ULL, header.size, "Should return empty/invalid header beyond EOF");

         // Test invalid size (too small)
         std::vector<uint8_t> dataSmall = {
             0x00, 0x00, 0x00, 0x04, // Size 4 (invalid)
             't', 'e', 's', 't'
         };
         io = std::make_shared<MemoryIOHandler>(dataSmall.data(), dataSmall.size());
         BoxParser parserSmall(io);

         header = parserSmall.ReadBoxHeader(0);
         ASSERT_EQUALS(0ULL, header.size, "Should mark header as invalid if size < 8");
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    try {
        TestSuite suite("BoxParser Tests");
        suite.addTest(std::make_unique<BoxParserTest>());

        std::vector<TestCaseInfo> results = suite.runAll();
        suite.printResults(results);

        return suite.getFailureCount(results) == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Test suite execution failed: " << e.what() << std::endl;
        return 1;
    }
}
