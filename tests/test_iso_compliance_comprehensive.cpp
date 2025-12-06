/*
 * test_iso_compliance_comprehensive.cpp - Comprehensive tests for ISO demuxer compliance validation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <memory>
#include <vector>
#include <cstring>

using namespace TestFramework;
using namespace PsyMP3::Demuxer::ISO;

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

// Helper functions for creating test data
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

// Test class for box structure validation
class BoxStructureValidationTest : public TestCase {
public:
    BoxStructureValidationTest() : TestCase("BoxStructureValidation") {}
    
protected:
    void runTest() override {
        // Test valid 32-bit box structure
        testValid32BitBoxStructure();
        
        // Test valid 64-bit box structure
        testValid64BitBoxStructure();
        
        // Test invalid box sizes
        testInvalidBoxSizes();
        
        // Test box size boundary conditions
        testBoxSizeBoundaryConditions();
        
        // Test box type validation
        testBoxTypeValidation();
    }
    
private:
    void testValid32BitBoxStructure() {
        std::vector<uint8_t> testData = createBoxHeader(BOX_FTYP, 32);
        testData.resize(32); // Fill with zeros
        
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_FTYP, 32, 0, 1000);
        ASSERT_TRUE(result.isValid, "Valid 32-bit box should pass validation");
        ASSERT_FALSE(result.is64BitSize, "32-bit box should not be marked as 64-bit");
        ASSERT_EQUALS(32ULL, result.actualSize, "Actual size should match input");
    }
    
    void testValid64BitBoxStructure() {
        uint64_t largeSize = 0x100000000ULL; // > 4GB
        std::vector<uint8_t> testData = create64BitBoxHeader(BOX_MDAT, largeSize);
        testData.resize(static_cast<size_t>(std::min(largeSize, static_cast<uint64_t>(1000)))); // Limit for testing
        
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_MDAT, largeSize, 0, largeSize + 100);
        ASSERT_TRUE(result.isValid, "Valid 64-bit box should pass validation");
        ASSERT_TRUE(result.is64BitSize, "Large box should be marked as 64-bit");
        ASSERT_EQUALS(largeSize, result.actualSize, "Actual size should match input");
    }
    
    void testInvalidBoxSizes() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test box size too small (less than header size)
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_FTYP, 4, 0, 1000);
        ASSERT_FALSE(result.isValid, "Box smaller than header should fail validation");
        
        // Test box size exceeding container
        result = validator.ValidateBoxStructure(BOX_FTYP, 2000, 0, 1000);
        ASSERT_FALSE(result.isValid, "Box exceeding container should fail validation");
        
        // Test zero size in middle of file (should be invalid)
        result = validator.ValidateBoxStructure(BOX_FTYP, 0, 100, 1000);
        ASSERT_FALSE(result.isValid, "Zero size box not at end should fail validation");
    }
    
    void testBoxSizeBoundaryConditions() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test minimum valid box size (8 bytes for header)
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_FTYP, 8, 0, 1000);
        ASSERT_TRUE(result.isValid, "Minimum box size should be valid");
        
        // Test maximum 32-bit size
        uint32_t maxSize = 0xFFFFFFFF;
        result = validator.ValidateBoxStructure(BOX_MDAT, maxSize, 0, static_cast<uint64_t>(maxSize) + 100);
        ASSERT_TRUE(result.isValid, "Maximum 32-bit size should be valid");
        
        // Test size exactly at container boundary
        result = validator.ValidateBoxStructure(BOX_FTYP, 1000, 0, 1000);
        ASSERT_TRUE(result.isValid, "Box exactly filling container should be valid");
    }
    
    void testBoxTypeValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test known box types
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_FTYP, 32, 0, 1000);
        ASSERT_TRUE(result.isValid, "Known box type should be valid");
        
        result = validator.ValidateBoxStructure(BOX_MOOV, 100, 0, 1000);
        ASSERT_TRUE(result.isValid, "Known box type should be valid");
        
        // Test unknown box type (should still validate structure)
        uint32_t unknownType = FOURCC('u', 'n', 'k', 'n');
        result = validator.ValidateBoxStructure(unknownType, 32, 0, 1000);
        ASSERT_TRUE(result.isValid, "Unknown box type should still validate structure");
    }
};

// Test class for 32-bit and 64-bit size validation
class BoxSizeValidationTest : public TestCase {
public:
    BoxSizeValidationTest() : TestCase("BoxSizeValidation") {}
    
protected:
    void runTest() override {
        test32BitSizeValidation();
        test64BitSizeValidation();
        testSizeTransitionBoundary();
        testSpecialSizeValues();
    }
    
private:
    void test32BitSizeValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test valid 32-bit sizes
        ASSERT_TRUE(validator.Validate32BitBoxSize(8, 0, 1000), "Minimum valid 32-bit size");
        ASSERT_TRUE(validator.Validate32BitBoxSize(100, 0, 1000), "Normal 32-bit size");
        ASSERT_TRUE(validator.Validate32BitBoxSize(1000, 0, 1000), "Maximum fitting 32-bit size");
        ASSERT_TRUE(validator.Validate32BitBoxSize(0, 0, 1000), "Zero size (extends to end)");
        
        // Test invalid 32-bit sizes
        ASSERT_FALSE(validator.Validate32BitBoxSize(4, 0, 1000), "Size too small for header");
        ASSERT_FALSE(validator.Validate32BitBoxSize(2000, 0, 1000), "Size exceeds container");
        ASSERT_FALSE(validator.Validate32BitBoxSize(7, 0, 1000), "Size smaller than minimum header");
    }
    
    void test64BitSizeValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test valid 64-bit sizes
        ASSERT_TRUE(validator.Validate64BitBoxSize(16, 0, 10000), "Minimum valid 64-bit size");
        ASSERT_TRUE(validator.Validate64BitBoxSize(0x100000000ULL, 0, 0x200000000ULL), "Large 64-bit size");
        ASSERT_TRUE(validator.Validate64BitBoxSize(0, 0, 10000), "Zero size (extends to end)");
        
        // Test invalid 64-bit sizes
        ASSERT_FALSE(validator.Validate64BitBoxSize(8, 0, 1000), "Size too small for 64-bit header");
        ASSERT_FALSE(validator.Validate64BitBoxSize(15, 0, 1000), "Size smaller than 64-bit header");
        ASSERT_FALSE(validator.Validate64BitBoxSize(2000, 0, 1000), "Size exceeds container");
    }
    
    void testSizeTransitionBoundary() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test sizes around 4GB boundary
        uint64_t fourGB = 0x100000000ULL;
        
        // Just under 4GB should be valid for 32-bit
        ASSERT_TRUE(validator.Validate32BitBoxSize(static_cast<uint32_t>(fourGB - 1), 0, fourGB + 100), 
                   "Size just under 4GB should be valid for 32-bit");
        
        // At 4GB should require 64-bit
        ASSERT_TRUE(validator.Validate64BitBoxSize(fourGB, 0, fourGB + 100), 
                   "Size at 4GB should be valid for 64-bit");
    }
    
    void testSpecialSizeValues() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test size = 1 (indicates 64-bit size follows)
        ASSERT_FALSE(validator.Validate32BitBoxSize(1, 0, 1000), "Size 1 should be invalid for 32-bit validation");
        
        // Test maximum values
        ASSERT_TRUE(validator.Validate32BitBoxSize(0xFFFFFFFF, 0, 0x100000000ULL), "Maximum 32-bit value");
        ASSERT_TRUE(validator.Validate64BitBoxSize(0xFFFFFFFFFFFFFFFFULL, 0, 0xFFFFFFFFFFFFFFFFULL), "Maximum 64-bit value");
    }
};

// Test class for timestamp and timescale validation
class TimestampValidationTest : public TestCase {
public:
    TimestampValidationTest() : TestCase("TimestampValidation") {}
    
protected:
    void runTest() override {
        testValidTimestampConfigurations();
        testInvalidTimescaleValues();
        testTimestampBoundaryConditions();
        testTimescaleStandardValues();
        testTimestampOverflow();
    }
    
private:
    void testValidTimestampConfigurations() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test common audio timescales
        TimestampValidationResult result = validator.ValidateTimestampConfiguration(44100, 44100, 88200);
        ASSERT_TRUE(result.isValid, "44.1kHz timescale should be valid");
        ASSERT_TRUE(result.hasValidTimescale, "44.1kHz should be recognized as valid timescale");
        
        result = validator.ValidateTimestampConfiguration(48000, 48000, 96000);
        ASSERT_TRUE(result.isValid, "48kHz timescale should be valid");
        
        result = validator.ValidateTimestampConfiguration(8000, 8000, 16000);
        ASSERT_TRUE(result.isValid, "8kHz timescale (telephony) should be valid");
        
        // Test movie timescale
        result = validator.ValidateTimestampConfiguration(1000, 1000, 2000);
        ASSERT_TRUE(result.isValid, "1000 timescale should be valid");
    }
    
    void testInvalidTimescaleValues() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test zero timescale
        TimestampValidationResult result = validator.ValidateTimestampConfiguration(1000, 0, 2000);
        ASSERT_FALSE(result.isValid, "Zero timescale should be invalid");
        ASSERT_FALSE(result.hasValidTimescale, "Zero timescale should not be recognized as valid");
        
        // Test extremely large timescale
        result = validator.ValidateTimestampConfiguration(1000, 0xFFFFFFFF, 2000);
        ASSERT_FALSE(result.isValid, "Extremely large timescale should be invalid");
    }
    
    void testTimestampBoundaryConditions() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test timestamp at duration boundary
        TimestampValidationResult result = validator.ValidateTimestampConfiguration(88200, 44100, 88200);
        ASSERT_TRUE(result.isValid, "Timestamp at duration boundary should be valid");
        
        // Test timestamp exceeding duration
        result = validator.ValidateTimestampConfiguration(100000, 44100, 88200);
        ASSERT_FALSE(result.isValid, "Timestamp exceeding duration should be invalid");
        
        // Test zero timestamp
        result = validator.ValidateTimestampConfiguration(0, 44100, 88200);
        ASSERT_TRUE(result.isValid, "Zero timestamp should be valid");
        
        // Test maximum timestamp values
        result = validator.ValidateTimestampConfiguration(0xFFFFFFFFFFFFFFFFULL, 44100, 0xFFFFFFFFFFFFFFFFULL);
        ASSERT_TRUE(result.isValid, "Maximum timestamp should be valid if within duration");
    }
    
    void testTimescaleStandardValues() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test standard audio sample rates
        std::vector<uint32_t> standardRates = {8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000};
        
        for (uint32_t rate : standardRates) {
            TimestampValidationResult result = validator.ValidateTimestampConfiguration(rate, rate, rate * 2);
            ASSERT_TRUE(result.isValid, "Standard sample rate " + std::to_string(rate) + " should be valid");
        }
        
        // Test non-standard but valid rates
        TimestampValidationResult result = validator.ValidateTimestampConfiguration(12000, 12000, 24000);
        ASSERT_TRUE(result.isValid, "Non-standard but reasonable rate should be valid");
    }
    
    void testTimestampOverflow() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test potential overflow conditions
        uint64_t largeTimestamp = 0x7FFFFFFFFFFFFFFFULL;
        uint32_t timescale = 1000000;
        uint64_t duration = 0x7FFFFFFFFFFFFFFFULL;
        
        TimestampValidationResult result = validator.ValidateTimestampConfiguration(largeTimestamp, timescale, duration);
        ASSERT_TRUE(result.isValid, "Large but valid timestamp should not overflow");
        
        // Test timestamp normalization
        ASSERT_TRUE(result.normalizedTimestamp <= duration, "Normalized timestamp should not exceed duration");
    }
};

// Test class for sample table consistency validation
class SampleTableConsistencyTest : public TestCase {
public:
    SampleTableConsistencyTest() : TestCase("SampleTableConsistency") {}
    
protected:
    void runTest() override {
        testValidSampleTableConfiguration();
        testInconsistentSampleCounts();
        testInvalidChunkReferences();
        testSampleToChunkConsistency();
        testTimeToSampleConsistency();
        testEmptySampleTables();
    }
    
private:
    void testValidSampleTableConfiguration() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Create consistent sample table
        SampleTableInfo sampleTable;
        sampleTable.chunkOffsets = {1000, 2000, 3000}; // 3 chunks
        sampleTable.sampleSizes = {100, 100, 100, 100, 100, 100}; // 6 samples
        sampleTable.sampleTimes = {0, 1024, 2048, 3072, 4096, 5120}; // 6 time entries
        
        // 2 samples per chunk for 3 chunks = 6 samples total
        SampleToChunkEntry entry = {1, 2, 1};
        sampleTable.sampleToChunkEntries = {entry};
        
        ASSERT_TRUE(validator.ValidateSampleTableConsistency(sampleTable), 
                   "Valid sample table should pass consistency check");
    }
    
    void testInconsistentSampleCounts() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        SampleTableInfo sampleTable;
        sampleTable.chunkOffsets = {1000, 2000, 3000}; // 3 chunks
        sampleTable.sampleSizes = {100, 100, 100, 100, 100, 100, 100}; // 7 samples
        sampleTable.sampleTimes = {0, 1024, 2048, 3072, 4096, 5120}; // 6 time entries
        
        // 2 samples per chunk for 3 chunks = 6 samples, but we have 7 sample sizes
        SampleToChunkEntry entry = {1, 2, 1};
        sampleTable.sampleToChunkEntries = {entry};
        
        ASSERT_FALSE(validator.ValidateSampleTableConsistency(sampleTable), 
                    "Inconsistent sample count should fail validation");
    }
    
    void testInvalidChunkReferences() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        SampleTableInfo sampleTable;
        sampleTable.chunkOffsets = {1000, 2000}; // 2 chunks
        sampleTable.sampleSizes = {100, 100, 100, 100}; // 4 samples
        sampleTable.sampleTimes = {0, 1024, 2048, 3072}; // 4 time entries
        
        // Reference to chunk 3, but only 2 chunks exist
        SampleToChunkEntry entry = {3, 2, 1};
        sampleTable.sampleToChunkEntries = {entry};
        
        ASSERT_FALSE(validator.ValidateSampleTableConsistency(sampleTable), 
                    "Invalid chunk reference should fail validation");
    }
    
    void testSampleToChunkConsistency() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test multiple sample-to-chunk entries
        SampleTableInfo sampleTable;
        sampleTable.chunkOffsets = {1000, 2000, 3000, 4000}; // 4 chunks
        sampleTable.sampleSizes = {100, 100, 100, 100, 100, 100, 100}; // 7 samples
        sampleTable.sampleTimes = {0, 1024, 2048, 3072, 4096, 5120, 6144}; // 7 time entries
        
        // Chunks 1-2: 2 samples each, Chunks 3-4: 1.5 samples each (should be invalid)
        SampleToChunkEntry entry1 = {1, 2, 1}; // Chunks 1-2: 2 samples each = 4 samples
        SampleToChunkEntry entry2 = {3, 1, 1}; // Chunks 3-4: 1 sample each = 2 samples
        // Total: 4 + 2 = 6 samples, but we have 7
        sampleTable.sampleToChunkEntries = {entry1, entry2};
        
        ASSERT_FALSE(validator.ValidateSampleTableConsistency(sampleTable), 
                    "Inconsistent sample-to-chunk mapping should fail validation");
    }
    
    void testTimeToSampleConsistency() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        SampleTableInfo sampleTable;
        sampleTable.chunkOffsets = {1000}; // 1 chunk
        sampleTable.sampleSizes = {100, 100, 100}; // 3 samples
        sampleTable.sampleTimes = {0, 1024}; // Only 2 time entries for 3 samples
        
        SampleToChunkEntry entry = {1, 3, 1}; // 3 samples in chunk 1
        sampleTable.sampleToChunkEntries = {entry};
        
        ASSERT_FALSE(validator.ValidateSampleTableConsistency(sampleTable), 
                    "Insufficient time entries should fail validation");
    }
    
    void testEmptySampleTables() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test completely empty sample table
        SampleTableInfo emptySampleTable;
        ASSERT_FALSE(validator.ValidateSampleTableConsistency(emptySampleTable), 
                    "Empty sample table should fail validation");
        
        // Test partially empty sample table
        SampleTableInfo partialSampleTable;
        partialSampleTable.chunkOffsets = {1000};
        // Missing other required tables
        ASSERT_FALSE(validator.ValidateSampleTableConsistency(partialSampleTable), 
                    "Partially empty sample table should fail validation");
    }
};

// Test class for codec-specific data integrity
class CodecDataIntegrityTest : public TestCase {
public:
    CodecDataIntegrityTest() : TestCase("CodecDataIntegrity") {}
    
protected:
    void runTest() override {
        testAACCodecValidation();
        testALACCodecValidation();
        testTelephonyCodecValidation();
        testPCMCodecValidation();
        testUnknownCodecValidation();
        testCorruptedCodecData();
    }
    
private:
    void testAACCodecValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "aac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Test valid AAC configuration (AudioSpecificConfig)
        std::vector<uint8_t> validAACConfig = {0x12, 0x10}; // LC profile, 44.1kHz, stereo
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("aac", validAACConfig, track), 
                   "Valid AAC configuration should pass validation");
        
        // Test missing AAC configuration
        std::vector<uint8_t> emptyConfig;
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", emptyConfig, track), 
                    "Missing AAC configuration should fail validation");
        
        // Test invalid AAC configuration (too short)
        std::vector<uint8_t> shortConfig = {0x12};
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", shortConfig, track), 
                    "Incomplete AAC configuration should fail validation");
    }
    
    void testALACCodecValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "alac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Test valid ALAC magic cookie (simplified)
        std::vector<uint8_t> validALACConfig = {
            0x00, 0x00, 0x00, 0x24, // Size
            'a', 'l', 'a', 'c',     // Type
            0x00, 0x00, 0x00, 0x00, // Version/flags
            0x00, 0x00, 0x10, 0x00, // Frame length
            0x10,                   // Bit depth
            0x02,                   // Channels
            // ... more ALAC-specific data
        };
        validALACConfig.resize(36); // Standard ALAC config size
        
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("alac", validALACConfig, track), 
                   "Valid ALAC configuration should pass validation");
        
        // Test missing ALAC configuration
        std::vector<uint8_t> emptyConfig;
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("alac", emptyConfig, track), 
                    "Missing ALAC configuration should fail validation");
    }
    
    void testTelephonyCodecValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test valid mulaw configuration
        AudioTrackInfo mulawTrack;
        mulawTrack.codecType = "ulaw";
        mulawTrack.sampleRate = 8000;
        mulawTrack.channelCount = 1;
        mulawTrack.bitsPerSample = 8;
        
        std::vector<uint8_t> emptyConfig; // Telephony codecs don't need config data
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, mulawTrack), 
                   "Valid mulaw configuration should pass validation");
        
        // Test valid alaw configuration
        AudioTrackInfo alawTrack;
        alawTrack.codecType = "alaw";
        alawTrack.sampleRate = 8000;
        alawTrack.channelCount = 1;
        alawTrack.bitsPerSample = 8;
        
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("alaw", emptyConfig, alawTrack), 
                   "Valid alaw configuration should pass validation");
        
        // Test invalid telephony configuration (wrong bit depth)
        mulawTrack.bitsPerSample = 16;
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, mulawTrack), 
                    "Invalid mulaw bit depth should fail validation");
        
        // Test invalid telephony configuration (wrong sample rate)
        mulawTrack.bitsPerSample = 8;
        mulawTrack.sampleRate = 44100;
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, mulawTrack), 
                    "Invalid mulaw sample rate should fail validation");
        
        // Test invalid telephony configuration (stereo)
        mulawTrack.sampleRate = 8000;
        mulawTrack.channelCount = 2;
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("ulaw", emptyConfig, mulawTrack), 
                    "Stereo telephony codec should fail validation");
    }
    
    void testPCMCodecValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "lpcm";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        std::vector<uint8_t> emptyConfig; // PCM doesn't need config data
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                   "Valid PCM configuration should pass validation");
        
        // Test various valid PCM configurations
        track.bitsPerSample = 24;
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                   "24-bit PCM should be valid");
        
        track.bitsPerSample = 32;
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                   "32-bit PCM should be valid");
        
        // Test invalid PCM configuration
        track.bitsPerSample = 7; // Invalid bit depth
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("lpcm", emptyConfig, track), 
                    "Invalid PCM bit depth should fail validation");
    }
    
    void testUnknownCodecValidation() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "unknown";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        std::vector<uint8_t> someConfig = {0x01, 0x02, 0x03};
        
        // Unknown codecs should pass basic validation (graceful handling)
        ASSERT_TRUE(validator.ValidateCodecDataIntegrity("unknown", someConfig, track), 
                   "Unknown codec should pass basic validation");
    }
    
    void testCorruptedCodecData() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        AudioTrackInfo track;
        track.codecType = "aac";
        track.sampleRate = 44100;
        track.channelCount = 2;
        track.bitsPerSample = 16;
        
        // Test corrupted AAC config (invalid profile)
        std::vector<uint8_t> corruptedConfig = {0xFF, 0xFF}; // Invalid values
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", corruptedConfig, track), 
                    "Corrupted AAC configuration should fail validation");
        
        // Test mismatched track info and codec config
        std::vector<uint8_t> mismatchedConfig = {0x11, 0x90}; // 48kHz config but track says 44.1kHz
        ASSERT_FALSE(validator.ValidateCodecDataIntegrity("aac", mismatchedConfig, track), 
                    "Mismatched codec configuration should fail validation");
    }
};

// Test class for container format compliance
class ContainerFormatComplianceTest : public TestCase {
public:
    ContainerFormatComplianceTest() : TestCase("ContainerFormatCompliance") {}
    
protected:
    void runTest() override {
        testMP4ContainerCompliance();
        testM4AContainerCompliance();
        testMOVContainerCompliance();
        test3GPContainerCompliance();
        testInvalidContainerFormats();
        testMissingRequiredBoxes();
    }
    
private:
    void testMP4ContainerCompliance() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Create valid MP4 file type box data
        std::vector<uint8_t> mp4FtypData = {
            'i', 's', 'o', 'm', // Major brand: isom
            0, 0, 0, 1,         // Minor version
            'i', 's', 'o', 'm', // Compatible brand: isom
            'm', 'p', '4', '1', // Compatible brand: mp41
            'm', 'p', '4', '2'  // Compatible brand: mp42
        };
        
        bool result = validator.ValidateContainerCompliance(mp4FtypData, "MP4");
        ASSERT_TRUE(result, "MP4 container should have basic compliance");
    }
    
    void testM4AContainerCompliance() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Create valid M4A file type box data
        std::vector<uint8_t> m4aFtypData = {
            'M', '4', 'A', ' ', // Major brand: M4A
            0, 0, 0, 0,         // Minor version
            'M', '4', 'A', ' ', // Compatible brand: M4A
            'm', 'p', '4', '2', // Compatible brand: mp42
            'i', 's', 'o', 'm'  // Compatible brand: isom
        };
        
        bool result = validator.ValidateContainerCompliance(m4aFtypData, "M4A");
        ASSERT_TRUE(result, "M4A container should have basic compliance");
    }
    
    void testMOVContainerCompliance() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Create valid MOV file type box data (QuickTime)
        std::vector<uint8_t> movFtypData = {
            'q', 't', ' ', ' ', // Major brand: qt
            0, 0, 0, 0,         // Minor version
            'q', 't', ' ', ' '  // Compatible brand: qt
        };
        
        bool result = validator.ValidateContainerCompliance(movFtypData, "MOV");
        ASSERT_TRUE(result, "MOV container should have basic compliance");
    }
    
    void test3GPContainerCompliance() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Create valid 3GP file type box data
        std::vector<uint8_t> threegpFtypData = {
            '3', 'g', 'p', '4', // Major brand: 3gp4
            0, 0, 0, 0,         // Minor version
            '3', 'g', 'p', '4', // Compatible brand: 3gp4
            'i', 's', 'o', 'm'  // Compatible brand: isom
        };
        
        bool result = validator.ValidateContainerCompliance(threegpFtypData, "3GP");
        ASSERT_TRUE(result, "3GP container should have basic compliance");
    }
    
    void testInvalidContainerFormats() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test empty file type box
        std::vector<uint8_t> emptyFtyp;
        bool result = validator.ValidateContainerCompliance(emptyFtyp, "MP4");
        ASSERT_FALSE(result, "Empty file type box should fail compliance");
        
        // Test truncated file type box
        std::vector<uint8_t> truncatedFtyp = {'i', 's', 'o'}; // Too short
        result = validator.ValidateContainerCompliance(truncatedFtyp, "MP4");
        ASSERT_FALSE(result, "Truncated file type box should fail compliance");
        
        // Test unknown container format
        std::vector<uint8_t> unknownFtyp = {
            'u', 'n', 'k', 'n', // Unknown brand
            0, 0, 0, 1,
            'u', 'n', 'k', 'n'
        };
        result = validator.ValidateContainerCompliance(unknownFtyp, "UNKNOWN");
        ASSERT_FALSE(result, "Unknown container format should fail compliance");
    }
    
    void testMissingRequiredBoxes() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<MockIOHandler>(testData);
        ComplianceValidator validator(mockIO);
        
        // Test container with valid ftyp but missing other required boxes
        std::vector<uint8_t> validFtyp = {
            'i', 's', 'o', 'm',
            0, 0, 0, 1,
            'i', 's', 'o', 'm',
            'm', 'p', '4', '1'
        };
        
        bool result = validator.ValidateContainerCompliance(validFtyp, "MP4");
        
        // Basic validation should pass even if complete structure is missing
        ASSERT_TRUE(result, "Valid ftyp should pass basic validation");
    }
};

int main() {
    TestSuite suite("ISO Demuxer Compliance Validation Comprehensive Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<BoxStructureValidationTest>());
    suite.addTest(std::make_unique<BoxSizeValidationTest>());
    suite.addTest(std::make_unique<TimestampValidationTest>());
    suite.addTest(std::make_unique<SampleTableConsistencyTest>());
    suite.addTest(std::make_unique<CodecDataIntegrityTest>());
    suite.addTest(std::make_unique<ContainerFormatComplianceTest>());
    
    // Run all tests
    std::vector<TestCaseInfo> results = suite.runAll();
    
    // Print results
    suite.printResults(results);
    
    // Return appropriate exit code
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}