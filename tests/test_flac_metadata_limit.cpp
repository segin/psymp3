/*
 * test_flac_metadata_limit.cpp - Verification test for FLAC Metadata allocation limit
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "demuxer/iso/BoxParser.h"
#include "io/MemoryIOHandler.h"
#include <vector>
#include <iostream>

using namespace PsyMP3;
using namespace PsyMP3::Demuxer::ISO;
using namespace PsyMP3::IO;

int main() {
    // 1. Create a large buffer (e.g. 40MB) representing a dfLa box content
    // We want to test that ParseFLACConfiguration rejects sizes > LIMIT (e.g. 32MB)
    // So we'll pass a size of 40MB.

    size_t testSize = 40 * 1024 * 1024; // 40MB
    std::vector<uint8_t> buffer;
    try {
        buffer.resize(testSize, 0);
    } catch (const std::bad_alloc& e) {
        std::cout << "Test setup failed: could not allocate 40MB buffer." << std::endl;
        return 1;
    }

    // dfLa box format:
    // - version (1 byte)
    // - flags (3 bytes)
    // - FLAC metadata blocks (remaining bytes)

    // Set up version/flags (valid values)
    buffer[0] = 0; // Version
    buffer[1] = 0; buffer[2] = 0; buffer[3] = 0; // Flags

    // Construct valid STREAMINFO block at start of metadata
    // Metadata Block Header at offset 4
    // Byte 0: Last Block (bit 7) + Block Type (bits 6-0) = 0x80 (Last Block, STREAMINFO)
    buffer[4] = 0x80;

    // Byte 1-3: Length (24 bits) = 34
    buffer[5] = 0; buffer[6] = 0; buffer[7] = 34;

    // STREAMINFO content (34 bytes starting at offset 8)
    // We leave as zeros which parses as valid (minimal) STREAMINFO

    auto io = std::make_shared<MemoryIOHandler>(buffer.data(), buffer.size());
    BoxParser parser(io);
    AudioTrackInfo track;

    std::cout << "Testing ParseFLACConfiguration with " << testSize << " bytes..." << std::endl;

    // In vulnerable version:
    // 1. Allocates 40MB (succeeds)
    // 2. Reads 40MB (succeeds)
    // 3. Parses STREAMINFO (succeeds)
    // 4. Returns true

    // In fixed version:
    // 1. Checks size > LIMIT (32MB) -> Fails
    // 2. Returns false

    bool result = parser.ParseFLACConfiguration(0, testSize, track);

    if (result) {
        std::cout << "VULNERABLE: ParseFLACConfiguration accepted " << testSize << " bytes." << std::endl;
        return 1; // Vulnerable / Test Failed (if expecting fix)
    } else {
        std::cout << "SECURE: ParseFLACConfiguration rejected " << testSize << " bytes." << std::endl;
        return 0; // Fixed / Test Passed
    }
}
