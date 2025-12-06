/*
 * test_iso_compliance_validation.cpp - Tests for ISO demuxer compliance validation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

// Mock IOHandler for testing
class MockIOHandler : public IOHandler {
private:
    std::vector<uint8_t> data;
    size_t position = 0;
    
public:
    MockIOHandler(const std::vector<uint8_t>& testData) : data(testData) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t totalBytes = size * count;
        size_t availableBytes = data.size() - position;
        size_t bytesToRead = std::min(totalBytes, availableBytes);
        
        if (bytesToRead > 0) {
            std::memcpy(buffer, data.data() + position, bytesToRead);
            position += bytesToRead;
        }
        
        return bytesToRead / size;
    }
    
    int seek(off_t offset, int whence) override {
        switch (whence) {
            case SEEK_SET:
                position = static_cast<size_t>(offset);
                break;
            case SEEK_CUR:
                position += static_cast<size_t>(offset);
                break;
            case SEEK_END:
                position = data.size() + static_cast<size_t>(offset);
                break;
        }
        
        if (position > data.size()) {
            position = data.size();
        }
        
        return 0;
    }
    
    off_t tell() override {
        return static_cast<off_t>(position);
    }
    
    bool eof() override {
        return position >= data.size();
    }
};

// Helper function to create box header bytes
std::vector<uint8_t> createBoxHeader(uint32_t type, uint32_t size) {
    std::vector<uint8_t> header(8);
    
    // Size (big-endian)
    header[0] = (size >> 24) & 0xFF;
    header[1] = (size >> 16) & 0xFF;
    header[2] = (size >> 8) & 0xFF;
    header[3] = size & 0xFF;
    
    // Type (big-endian)
    header[4] = (type >> 24) & 0xFF;
    header[5] = (type >> 16) & 0xFF;
    header[6] = (type >> 8) & 0xFF;
    header[7] = type & 0xFF;
    
    return header;
}

// Helper function to create 64-bit box header
std::vector<uint8_t> create64BitBoxHeader(uint32_t type, uint64_t size) {
    std::vector<uint8_t> header(16);
    
    // Size = 1 (indicates 64-bit size follows)
    header[0] = 0;
    header[1] = 0;
    header[2] = 0;
    header[3] = 1;
    
    // Type (big-endian)
    header[4] = (type >> 24) & 0xFF;
    header[5] = (type >> 16) & 0xFF;
    header[6] = (type >> 8) & 0xFF;
    header[7] = type & 0xFF;
    
    // 64-bit size (big-endian)
    header[8] = (size >> 56) & 0xFF;
    header[9] = (size >> 48) & 0xFF;
    header[10] = (size >> 40) & 0xFF;
    header[11] = (size >> 32) & 0xFF;
    header[12] = (size >> 24) & 0xFF;
    header[13] = (size >> 16) & 0xFF;
    header[14] = (size >> 8) & 0xFF;
    header[15] = size & 0xFF;
    
    return header;
}

// Test box structure validation
void testBoxStructureValidation() {
    std::cout << "Testing box structure validation..." << std::endl;
    
    std::vector<uint8_t> testData = createBoxHeader(BOX_FTYP, 32);
    testData.resize(32); // Fill with zeros
    
    auto mockIO = std::make_shared<MockIOHandler>(testData);
    ComplianceValidator validator(mockIO);
    
    // Test valid 32-bit box
    BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_FTYP, 32, 0, 1000);
    assert(result.isValid);
    assert(!result.is64BitSize);
    assert(result.actualSize == 32);
    
    // Test invalid box size (too small)
    result = validator.ValidateBoxStructure(BOX_FTYP, 4, 0, 1000);
    assert(!result.isValid);
    
    std::cout << "Box structure validation tests passed!" << std::endl;
}

// Test 32-bit and 64-bit box size validation
void testBoxSizeValidation() {
    std::cout << "Testing 32-bit and 64-bit box size validation..." << std::endl;
    
    std::vector<uint8_t> testData;
    auto mockIO = std::make_shared<MockIOHandler>(testData);
    ComplianceValidator validator(mockIO);
    
    // Test valid 32-bit sizes
    assert(validator.Validate32BitBoxSize(8, 0, 1000));
    assert(validator.Validate32BitBoxSize(100, 0, 1000));
    assert(validator.Validate32BitBoxSize(0, 0, 1000)); // Size 0 is valid (extends to end)
    
    // Test invalid 32-bit sizes
    assert(!validator.Validate32BitBoxSize(4, 0, 1000)); // Too small
    assert(!validator.Validate32BitBoxSize(2000, 0, 1000)); // Exceeds container
    
    // Test valid 64-bit sizes
    assert(validator.Validate64BitBoxSize(16, 0, 10000));
    assert(validator.Validate64BitBoxSize(0x100000000ULL, 0, 0x200000000ULL)); // > 4GB
    
    // Test invalid 64-bit sizes
    assert(!validator.Validate64BitBoxSize(8, 0, 1000)); // Too small for 64-bit
    assert(!validator.Validate64BitBoxSize(2000, 0, 1000)); // Exceeds container
    
    std::cout << "Box size validation tests passed!" << std::endl;
}

// Test timestamp and timescale validation
void testTimestampValidation() {
    std::cout << "Testing timestamp and timescale validation..." << std::endl;
    
    std::vector<uint8_t> testData;
    auto mockIO = std::make_shared<MockIOHandler>(testData);
    ComplianceValidator validator(mockIO);
    
    // Test valid timestamp configurations
    TimestampValidationResult result = validator.ValidateTimestampConfiguration(44100, 44100, 88200);
    assert(result.isValid);
    assert(result.hasValidTimescale);
    
    result = validator.ValidateTimestampConfiguration(48000, 48000, 96000);
    assert(result.isValid);
    
    // Test invalid timescale
    result = validator.ValidateTimestampConfiguration(1000, 0, 2000);
    assert(!result.isValid);
    assert(!result.hasValidTimescale);
    
    // Test timestamp exceeding duration
    result = validator.ValidateTimestampConfiguration(100000, 44100, 50000);
    assert(!result.isValid);
    
    std::cout << "Timestamp validation tests passed!" << std::endl;
}

// Test sample table consistency validation
void testSampleTableValidation() {
    std::cout << "Testing sample table consistency validation..." << std::endl;
    
    std::vector<uint8_t> testData;
    auto mockIO = std::make_shared<MockIOHandler>(testData);
    ComplianceValidator validator(mockIO);
    
    // Create valid sample table info
    SampleTableInfo sampleTable;
    sampleTable.chunkOffsets = {1000, 2000, 3000};
    sampleTable.sampleSizes = {100, 100, 100, 100, 100, 100}; // 6 samples
    sampleTable.sampleTimes = {0, 1024, 2048, 3072, 4096, 5120}; // 6 time entries
    
    // Create sample-to-chunk entries: 2 samples per chunk for 3 chunks = 6 samples
    SampleToChunkEntry entry1 = {1, 2, 1}; // Chunk 1-3: 2 samples per chunk
    sampleTable.sampleToChunkEntries = {entry1};
    
    assert(validator.ValidateSampleTableConsistency(sampleTable));
    
    // Test inconsistent sample count
    sampleTable.sampleSizes.push_back(100); // Now 7 samples, but still 6 from sample-to-chunk
    assert(!validator.ValidateSampleTableConsistency(sampleTable));
    
    std::cout << "Sample table validation tests passed!" << std::endl;
}

// Test codec data integrity validation
void testCodecDataValidation() {
    std::cout << "Testing codec data integrity validation..." << std::endl;
    
    std::vector<uint8_t> testData;
    auto mockIO = std::make_shared<MockIOHandler>(testData);
    ComplianceValidator validator(mockIO);
    
    // Create test track info
    AudioTrackInfo track;
    track.codecType = "aac";
    track.sampleRate = 44100;
    track.channelCount = 2;
    track.bitsPerSample = 16;
    
    // Test valid AAC configuration
    std::vector<uint8_t> aacConfig = {0x12, 0x10}; // Valid AudioSpecificConfig
    assert(validator.ValidateCodecDataIntegrity("aac", aacConfig, track));
    
    // Test missing AAC configuration
    std::vector<uint8_t> emptyConfig;
    assert(!validator.ValidateCodecDataIntegrity("aac", emptyConfig, track));
    
    // Test telephony codec
    track.codecType = "ulaw";
    track.sampleRate = 8000;
    track.channelCount = 1;
    track.bitsPerSample = 8;
    assert(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, track));
    
    // Test invalid telephony configuration
    track.bitsPerSample = 16; // Should be 8 for telephony
    assert(!validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, track));
    
    std::cout << "Codec data validation tests passed!" << std::endl;
}

// Test container compliance validation
void testContainerCompliance() {
    std::cout << "Testing container compliance validation..." << std::endl;
    
    std::vector<uint8_t> testData;
    auto mockIO = std::make_shared<MockIOHandler>(testData);
    ComplianceValidator validator(mockIO);
    
    // Create valid file type box data
    std::vector<uint8_t> ftypData = {
        'i', 's', 'o', 'm', // Major brand: isom
        0, 0, 0, 1,         // Minor version
        'i', 's', 'o', 'm', // Compatible brand: isom
        'm', 'p', '4', '1'  // Compatible brand: mp41
    };
    
    ComplianceValidationResult result = validator.ValidateContainerCompliance(ftypData, "MP4");
    // Note: This may not be fully compliant without moov box, but should not crash
    
    // Test empty file type box
    std::vector<uint8_t> emptyFtyp;
    result = validator.ValidateContainerCompliance(emptyFtyp, "MP4");
    assert(!result.isCompliant);
    
    std::cout << "Container compliance validation tests passed!" << std::endl;
}

// Test track compliance validation
void testTrackCompliance() {
    std::cout << "Testing track compliance validation..." << std::endl;
    
    std::vector<uint8_t> testData;
    auto mockIO = std::make_shared<MockIOHandler>(testData);
    ComplianceValidator validator(mockIO);
    
    // Create valid track
    AudioTrackInfo track;
    track.trackId = 1;
    track.codecType = "aac";
    track.sampleRate = 44100;
    track.channelCount = 2;
    track.bitsPerSample = 16;
    track.timescale = 44100;
    track.duration = 88200; // 2 seconds
    track.codecConfig = {0x12, 0x10}; // Valid AAC config
    
    // Create valid sample table
    track.sampleTableInfo.chunkOffsets = {1000};
    track.sampleTableInfo.sampleSizes = {100, 100};
    track.sampleTableInfo.sampleTimes = {0, 1024};
    SampleToChunkEntry entry = {1, 2, 1};
    track.sampleTableInfo.sampleToChunkEntries = {entry};
    
    ComplianceValidationResult result = validator.ValidateTrackCompliance(track);
    assert(result.isCompliant);
    
    // Test invalid track ID
    track.trackId = 0;
    result = validator.ValidateTrackCompliance(track);
    assert(!result.isCompliant);
    
    std::cout << "Track compliance validation tests passed!" << std::endl;
}

// Test compliance reporting
void testComplianceReporting() {
    std::cout << "Testing compliance reporting..." << std::endl;
    
    std::vector<uint8_t> testData;
    auto mockIO = std::make_shared<MockIOHandler>(testData);
    ComplianceValidator validator(mockIO);
    
    // Initially should be compliant (no errors)
    ComplianceValidationResult report = validator.GetComplianceReport();
    assert(report.isCompliant);
    assert(report.errors.empty());
    
    // Trigger some validation that adds warnings/errors
    validator.ValidateTimestampConfiguration(1000, 0, 2000); // Invalid timescale
    
    report = validator.GetComplianceReport();
    assert(!report.isCompliant); // Should now have errors
    assert(!report.errors.empty());
    
    std::cout << "Compliance reporting tests passed!" << std::endl;
}

// Test utility functions
void testUtilityFunctions() {
    std::cout << "Testing utility functions..." << std::endl;
    
    std::vector<uint8_t> testData;
    auto mockIO = std::make_shared<MockIOHandler>(testData);
    ComplianceValidator validator(mockIO);
    
    // Test box type to string conversion
    std::string ftypStr = validator.BoxTypeToString(BOX_FTYP);
    assert(ftypStr == "ftyp");
    
    std::string moovStr = validator.BoxTypeToString(BOX_MOOV);
    assert(moovStr == "moov");
    
    // Test required box checking
    assert(validator.IsRequiredBox(BOX_MVHD, BOX_MOOV)); // mvhd required in moov
    assert(!validator.IsRequiredBox(BOX_FTYP, BOX_MOOV)); // ftyp not required in moov
    
    // Test box nesting validation
    assert(validator.ValidateBoxNesting(BOX_MVHD, BOX_MOOV)); // mvhd allowed in moov
    assert(validator.ValidateBoxNesting(BOX_TRAK, BOX_MOOV)); // trak allowed in moov
    
    std::cout << "Utility function tests passed!" << std::endl;
}

int main() {
    std::cout << "Running ISO Demuxer Compliance Validation Tests..." << std::endl;
    
    try {
        testBoxStructureValidation();
        testBoxSizeValidation();
        testTimestampValidation();
        testSampleTableValidation();
        testCodecDataValidation();
        testContainerCompliance();
        testTrackCompliance();
        testComplianceReporting();
        testUtilityFunctions();
        
        std::cout << "\nAll ISO compliance validation tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}