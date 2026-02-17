/*
 * test_boxparser_unit.cpp - Unit tests for BoxParser
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
#include "demuxer/iso/BoxParser.h"
#include "demuxer/iso/ISODemuxer.h"
#include "io/MemoryIOHandler.h"
#include "io/IOHandler.h"
#include "debug.h"

#include <vector>
#include <memory>
#include <iostream>

using namespace PsyMP3::Demuxer::ISO;
using namespace PsyMP3::IO;

namespace {

// Helper to create a BoxParser with data
std::unique_ptr<BoxParser> createParser(const std::vector<uint8_t>& data) {
    auto io = std::make_shared<MemoryIOHandler>(data.data(), data.size());
    return std::make_unique<BoxParser>(io);
}

// Helper to write a big-endian uint32
void writeUInt32BE(std::vector<uint8_t>& data, uint32_t val) {
    data.push_back((val >> 24) & 0xFF);
    data.push_back((val >> 16) & 0xFF);
    data.push_back((val >> 8) & 0xFF);
    data.push_back(val & 0xFF);
}

// Helper to write a big-endian uint64
void writeUInt64BE(std::vector<uint8_t>& data, uint64_t val) {
    writeUInt32BE(data, (val >> 32) & 0xFFFFFFFF);
    writeUInt32BE(data, val & 0xFFFFFFFF);
}

void Test_ReadBoxHeader() {
    // 1. Standard Box (size 8, type 'test')
    {
        std::vector<uint8_t> data;
        writeUInt32BE(data, 8); // Size
        writeUInt32BE(data, FOURCC('t','e','s','t')); // Type

        auto parser = createParser(data);
        BoxHeader header = parser->ReadBoxHeader(0);

        ASSERT_EQUALS(8ULL, header.size, "Standard box size should be 8");
        ASSERT_EQUALS(FOURCC('t','e','s','t'), header.type, "Standard box type mismatch");
        ASSERT_FALSE(header.extendedSize, "Standard box should not be extended");
        ASSERT_EQUALS(8ULL, header.dataOffset, "Data offset should be 8");
    }

    // 2. Extended Box (size 1, type 'big ', extended size 20)
    {
        std::vector<uint8_t> data;
        writeUInt32BE(data, 1); // Size 1 means extended
        writeUInt32BE(data, FOURCC('b','i','g',' ')); // Type
        writeUInt64BE(data, 20); // Extended size
        // Padding to reach 20 bytes
        data.resize(20, 0);

        auto parser = createParser(data);
        BoxHeader header = parser->ReadBoxHeader(0);

        ASSERT_EQUALS(20ULL, header.size, "Extended box size should be 20");
        ASSERT_EQUALS(FOURCC('b','i','g',' '), header.type, "Extended box type mismatch");
        ASSERT_TRUE(header.extendedSize, "Extended box should be marked extended");
        ASSERT_EQUALS(16ULL, header.dataOffset, "Extended data offset should be 16");
    }

    // 3. Size to EOF (size 0)
    {
        std::vector<uint8_t> data;
        writeUInt32BE(data, 0); // Size 0 means to EOF
        writeUInt32BE(data, FOURCC('l','a','s','t')); // Type
        data.resize(100, 0); // Total size 100

        auto parser = createParser(data);
        BoxHeader header = parser->ReadBoxHeader(0);

        ASSERT_EQUALS(100ULL, header.size, "EOF box size should match file size");
        ASSERT_EQUALS(FOURCC('l','a','s','t'), header.type, "EOF box type mismatch");
        ASSERT_EQUALS(8ULL, header.dataOffset, "EOF box data offset should be 8");
    }
}

void Test_ValidateBoxSize() {
    std::vector<uint8_t> dummyData(100, 0);
    auto parser = createParser(dummyData); // File size 100

    // 1. Valid size
    BoxHeader validHeader;
    validHeader.size = 20;
    validHeader.extendedSize = false;
    validHeader.dataOffset = 8;
    ASSERT_TRUE(parser->ValidateBoxSize(validHeader, 100), "Valid box size rejected");

    // 2. Size > container
    ASSERT_FALSE(parser->ValidateBoxSize(validHeader, 10), "Box larger than container accepted");

    // 3. Size > file
    validHeader.size = 200;
    ASSERT_FALSE(parser->ValidateBoxSize(validHeader, 1000), "Box larger than file accepted");

    // 4. Too small (standard)
    validHeader.size = 4;
    ASSERT_FALSE(parser->ValidateBoxSize(validHeader, 100), "Box smaller than header accepted");

    // 5. Too small (extended)
    validHeader.extendedSize = true;
    validHeader.size = 12;
    ASSERT_FALSE(parser->ValidateBoxSize(validHeader, 100), "Extended box smaller than header accepted");

    // 6. Huge size (OOM protection) - assuming 1GB limit mentioned in BoxParser.cpp
    // Need a parser with huge file size to pass the initial file size check
    std::vector<uint8_t> tiny(1);
    auto hugeIO = std::make_shared<MemoryIOHandler>(tiny.data(), 10ULL * 1024 * 1024 * 1024, false); // 10GB fake size
    auto hugeParser = std::make_unique<BoxParser>(hugeIO);

    validHeader.size = 2ULL * 1024 * 1024 * 1024; // 2GB
    // Note: ValidateBoxSize returns true for large boxes but flags potential recovery in comments.
    // The code says: return true; // Let higher-level code handle this
    ASSERT_TRUE(hugeParser->ValidateBoxSize(validHeader, 10ULL * 1024 * 1024 * 1024), "Large box rejected (should rely on higher logic)");
}

void Test_ParseRecursively() {
    // Create MOOV -> TRAK -> TKHD structure
    std::vector<uint8_t> data;

    // MOOV box
    uint32_t moovSize = 8 + (8 + (8 + 84)); // MOOV header + TRAK header + TKHD (assuming minimal)
    writeUInt32BE(data, 0); // Placeholder for MOOV size
    writeUInt32BE(data, BOX_MOOV);

    // TRAK box
    writeUInt32BE(data, 0); // Placeholder for TRAK size
    writeUInt32BE(data, BOX_TRAK);

    // TKHD box
    writeUInt32BE(data, 0); // Placeholder for TKHD size
    writeUInt32BE(data, BOX_TKHD);
    data.resize(data.size() + 84, 0); // Payload for TKHD (version, flags, etc.)

    // Fix sizes
    uint32_t tkhdSize = 8 + 84;
    uint32_t trakSize = 8 + tkhdSize;
    moovSize = 8 + trakSize;

    // Rewrite sizes
    data[0] = (moovSize >> 24); data[1] = (moovSize >> 16); data[2] = (moovSize >> 8); data[3] = (moovSize & 0xFF);
    data[8] = (trakSize >> 24); data[9] = (trakSize >> 16); data[10] = (trakSize >> 8); data[11] = (trakSize & 0xFF);
    data[16] = (tkhdSize >> 24); data[17] = (tkhdSize >> 16); data[18] = (tkhdSize >> 8); data[19] = (tkhdSize & 0xFF);

    auto parser = createParser(data);

    int boxCount = 0;
    bool foundMoov = false;
    bool foundTrak = false;
    bool foundTkhd = false;

    parser->ParseBoxRecursively(0, data.size(), [&](const BoxHeader& header, uint64_t offset) -> bool {
        boxCount++;
        if (header.type == BOX_MOOV) {
            foundMoov = true;
            return parser->ParseBoxRecursively(header.dataOffset, header.size - 8, [&](const BoxHeader& subHeader, uint64_t subOffset) -> bool {
                 if (subHeader.type == BOX_TRAK) {
                     foundTrak = true;
                     return parser->ParseBoxRecursively(subHeader.dataOffset, subHeader.size - 8, [&](const BoxHeader& subSubHeader, uint64_t subSubOffset) -> bool {
                         if (subSubHeader.type == BOX_TKHD) {
                             foundTkhd = true;
                         }
                         return true;
                     });
                 }
                 return true;
            });
        }
        return true;
    });

    ASSERT_TRUE(foundMoov, "MOOV box not found");
    ASSERT_TRUE(foundTrak, "TRAK box not found");
    ASSERT_TRUE(foundTkhd, "TKHD box not found");
}

void Test_ParseTimeToSampleBox() {
    std::vector<uint8_t> data;
    // STTS header not needed for ParseTimeToSampleBox call directly, only payload
    // STTS payload: version(1) + flags(3) + entryCount(4) + entries(count * 8)

    writeUInt32BE(data, 0); // Version + Flags
    writeUInt32BE(data, 2); // 2 entries

    // Entry 1
    writeUInt32BE(data, 10); // Count
    writeUInt32BE(data, 1);  // Delta

    // Entry 2
    writeUInt32BE(data, 5);  // Count
    writeUInt32BE(data, 2);  // Delta

    auto parser = createParser(data);
    SampleTableInfo tables;

    bool result = parser->ParseTimeToSampleBox(0, data.size(), tables);

    ASSERT_TRUE(result, "ParseTimeToSampleBox failed");
    ASSERT_EQUALS(15ULL, tables.sampleTimes.size(), "Total sample count mismatch");

    // Verify specific times
    // First 10 samples have delta 1: 0, 1, 2... 9
    // Next 5 samples have delta 2: 10, 12, 14, 16, 18
    ASSERT_EQUALS(0ULL, tables.sampleTimes[0], "Sample 0 time mismatch");
    ASSERT_EQUALS(9ULL, tables.sampleTimes[9], "Sample 9 time mismatch");
    ASSERT_EQUALS(10ULL, tables.sampleTimes[10], "Sample 10 time mismatch");
    ASSERT_EQUALS(18ULL, tables.sampleTimes[14], "Sample 14 time mismatch");
}

void Test_ParseOOMProtection() {
    std::vector<uint8_t> data;
    writeUInt32BE(data, 0); // Version + Flags
    writeUInt32BE(data, 1); // 1 entry

    // Entry 1 with huge count
    writeUInt32BE(data, 20000000); // > MAX_SAMPLES_PER_TRACK (10M)
    writeUInt32BE(data, 1);

    auto parser = createParser(data);
    SampleTableInfo tables;

    bool result = parser->ParseTimeToSampleBox(0, data.size(), tables);

    ASSERT_FALSE(result, "Huge sample count should be rejected");
}

void Test_ParseFLACConfiguration() {
    std::vector<uint8_t> data;
    // dfLa box format: version(1) + flags(3) + FLAC Metadata Blocks

    data.push_back(0); // Version
    data.push_back(0); data.push_back(0); data.push_back(0); // Flags

    // FLAC STREAMINFO Block
    // Header: type(1) [7-bit] + isLast(1) [1-bit] -> Type 0 (STREAMINFO) is 0x00 or 0x80 (last)
    // Let's say it's last block: 0x80
    data.push_back(0x80);

    // Length (24-bit): 34 bytes
    data.push_back(0); data.push_back(0); data.push_back(34);

    // STREAMINFO Payload (34 bytes)
    // Min Block Size (2), Max Block Size (2), Min Frame (3), Max Frame (3) -> 10 bytes
    // Sample Rate (20), Channels (3), BPS (5), Total Samples (36) -> 8 bytes
    // MD5 (16)

    // Fill first 10 bytes with 0
    for(int i=0; i<10; i++) data.push_back(0);

    // Sample Rate: 44100 (0xAC44) -> 20 bits: 0000 1010 1100 0100 0100
    // Channels: 2
    // BPS: 16

    // Byte 10: SR[19-12] -> 0000 1010 -> 0x0A
    data.push_back(0x0A);
    // Byte 11: SR[11-4] -> 1100 0100 -> 0xC4
    data.push_back(0xC4);
    // Byte 12: SR[3-0] (0100) | CH[2-0] (001 -> 010) | BPS[4] (0) -> 0100 0010 -> 0x42
    data.push_back(0x42);
    // Byte 13: BPS[3-0] (1111) | TS[35-32] (0000) -> 1111 0000 -> 0xF0
    data.push_back(0xF0);

    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0); // Rest of total samples

    // MD5 (16 bytes)
    for(int i=0; i<16; i++) data.push_back(0);

    auto parser = createParser(data);
    AudioTrackInfo track;

    bool result = parser->ParseFLACConfiguration(0, data.size(), track);

    ASSERT_TRUE(result, "ParseFLACConfiguration failed");
    ASSERT_EQUALS(44100ULL, (uint64_t)track.sampleRate, "Sample rate mismatch");
    ASSERT_EQUALS(2, (int)track.channelCount, "Channel count mismatch");
    ASSERT_EQUALS(16, (int)track.bitsPerSample, "Bits per sample mismatch");
}

} // namespace

int main() {
    TestFramework::TestSuite suite("BoxParserTests");

    suite.addTest("ReadBoxHeader", Test_ReadBoxHeader);
    suite.addTest("ValidateBoxSize", Test_ValidateBoxSize);
    suite.addTest("ParseRecursively", Test_ParseRecursively);
    suite.addTest("ParseTimeToSampleBox", Test_ParseTimeToSampleBox);
    suite.addTest("ParseOOMProtection", Test_ParseOOMProtection);
    suite.addTest("ParseFLACConfiguration", Test_ParseFLACConfiguration);

    std::vector<TestFramework::TestCaseInfo> results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
