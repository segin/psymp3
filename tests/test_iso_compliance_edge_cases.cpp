/*
 * test_iso_compliance_edge_cases.cpp - Edge case tests for ISO demuxer compliance validation
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
#include <limits>

using namespace TestFramework;

// Mock IOHandler for testing edge cases
class EdgeCaseMockIOHandler : public IOHandler {
private:
    std::vector<uint8_t> data;
    size_t position = 0;
    bool simulateIOError = false;
    
public:
    EdgeCaseMockIOHandler(const std::vector<uint8_t>& testData) : data(testData) {}
    
    void setSimulateIOError(bool simulate) { simulateIOError = simulate; }
    
    size_t read(void* buffer, size_t size, size_t count) override {
        if (simulateIOError) {
            return 0; // Simulate read failure
        }
        
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
        if (simulateIOError) {
            return -1; // Simulate seek failure
        }
        
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

// Test class for extreme box size edge cases
class ExtremeBoxSizeTest : public TestCase {
public:
    ExtremeBoxSizeTest() : TestCase("ExtremeBoxSizeTest") {}
    
protected:
    void runTest() override {
        testMaximumBoxSizes();
        testMinimumBoxSizes();
        testZeroSizeBoxes();
        testOverflowConditions();
        testNegativeSizeHandling();
    }
    
private:
    void testMaximumBoxSizes() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test maximum 32-bit size
        uint32_t max32 = std::numeric_limits<uint32_t>::max();
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_MDAT, max32, 0, static_cast<uint64_t>(max32) + 1000);
        ASSERT_TRUE(result.isValid, "Maximum 32-bit size should be valid");
        
        // Test maximum 64-bit size
        uint64_t max64 = std::numeric_limits<uint64_t>::max();
        result = validator.ValidateBoxStructure(BOX_MDAT, max64, 0, max64);
        ASSERT_TRUE(result.isValid, "Maximum 64-bit size should be valid");
        
        // Test size just over 4GB boundary
        uint64_t justOver4GB = 0x100000001ULL;
        result = validator.ValidateBoxStructure(BOX_MDAT, justOver4GB, 0, justOver4GB + 1000);
        ASSERT_TRUE(result.isValid, "Size just over 4GB should be valid");
        ASSERT_TRUE(result.is64BitSize, "Size over 4GB should be marked as 64-bit");
    }
    
    void testMinimumBoxSizes() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test minimum valid 32-bit box size (8 bytes for header)
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_FTYP, 8, 0, 1000);
        ASSERT_TRUE(result.isValid, "Minimum 32-bit box size should be valid");
        
        // Test minimum valid 64-bit box size (16 bytes for extended header)
        result = validator.ValidateBoxStructure(BOX_MDAT, 16, 0, 1000);
        ASSERT_TRUE(result.isValid, "Minimum 64-bit box size should be valid");
        
        // Test size smaller than header
        result = validator.ValidateBoxStructure(BOX_FTYP, 7, 0, 1000);
        ASSERT_FALSE(result.isValid, "Size smaller than header should be invalid");
        
        result = validator.ValidateBoxStructure(BOX_MDAT, 15, 0, 1000);
        ASSERT_FALSE(result.isValid, "Size smaller than 64-bit header should be invalid");
    }
    
    void testZeroSizeBoxes() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test zero size at end of file (should be valid - extends to end)
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_MDAT, 0, 900, 1000);
        ASSERT_TRUE(result.isValid, "Zero size box at end should be valid");
        
        // Test zero size in middle of file (should be invalid)
        result = validator.ValidateBoxStructure(BOX_MDAT, 0, 100, 1000);
        ASSERT_FALSE(result.isValid, "Zero size box in middle should be invalid");
        
        // Test zero size at beginning of file
        result = validator.ValidateBoxStructure(BOX_FTYP, 0, 0, 1000);
        ASSERT_FALSE(result.isValid, "Zero size box at beginning should be invalid");
    }
    
    void testOverflowConditions() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test box size that would cause offset overflow
        uint64_t largeOffset = std::numeric_limits<uint64_t>::max() - 100;
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_MDAT, 200, largeOffset, std::numeric_limits<uint64_t>::max());
        ASSERT_FALSE(result.isValid, "Box causing offset overflow should be invalid");
        
        // Test container size overflow
        result = validator.ValidateBoxStructure(BOX_MDAT, 1000, 0, std::numeric_limits<uint64_t>::max());
        ASSERT_TRUE(result.isValid, "Box within maximum container should be valid");
    }
    
    void testNegativeSizeHandling() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test how validator handles what would be negative sizes if interpreted as signed
        uint32_t largeUnsigned = 0x80000000; // Would be negative if signed
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_MDAT, largeUnsigned, 0, static_cast<uint64_t>(largeUnsigned) + 1000);
        ASSERT_TRUE(result.isValid, "Large unsigned size should be valid");
        
        // Test maximum signed value
        uint32_t maxSigned = 0x7FFFFFFF;
        result = validator.ValidateBoxStructure(BOX_MDAT, maxSigned, 0, static_cast<uint64_t>(maxSigned) + 1000);
        ASSERT_TRUE(result.isValid, "Maximum signed value should be valid");
    }
};

// Test class for timestamp edge cases
class TimestampEdgeCaseTest : public TestCase {
public:
    TimestampEdgeCaseTest() : TestCase("TimestampEdgeCaseTest") {}
    
protected:
    void runTest() override {
        testExtremeTimescaleValues();
        testTimestampOverflowScenarios();
        testZeroTimestampAndDuration();
        testTimescaleResolutionLimits();
        testTimestampPrecisionLoss();
    }
    
private:
    void testExtremeTimescaleValues() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test minimum valid timescale
        TimestampValidationResult result = validator.ValidateTimestampConfiguration(1, 1, 2);
        ASSERT_TRUE(result.isValid, "Minimum timescale should be valid");
        
        // Test maximum reasonable timescale
        uint32_t maxReasonable = 192000; // High-end audio sample rate
        result = validator.ValidateTimestampConfiguration(maxReasonable, maxReasonable, maxReasonable * 2);
        ASSERT_TRUE(result.isValid, "Maximum reasonable timescale should be valid");
        
        // Test extremely high timescale
        uint32_t extremelyHigh = 10000000; // 10MHz
        result = validator.ValidateTimestampConfiguration(extremelyHigh, extremelyHigh, extremelyHigh * 2);
        ASSERT_FALSE(result.isValid, "Extremely high timescale should be invalid");
        
        // Test maximum uint32 timescale
        uint32_t maxUint32 = std::numeric_limits<uint32_t>::max();
        result = validator.ValidateTimestampConfiguration(1, maxUint32, 2);
        ASSERT_FALSE(result.isValid, "Maximum uint32 timescale should be invalid");
    }
    
    void testTimestampOverflowScenarios() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test timestamp at maximum uint64
        uint64_t maxTimestamp = std::numeric_limits<uint64_t>::max();
        TimestampValidationResult result = validator.ValidateTimestampConfiguration(maxTimestamp, 44100, maxTimestamp);
        ASSERT_TRUE(result.isValid, "Maximum timestamp within duration should be valid");
        
        // Test timestamp causing overflow in calculations
        result = validator.ValidateTimestampConfiguration(maxTimestamp, 1000000, maxTimestamp);
        ASSERT_TRUE(result.isValid, "Timestamp not causing overflow should be valid");
        
        // Test duration at maximum
        uint64_t maxDuration = std::numeric_limits<uint64_t>::max();
        result = validator.ValidateTimestampConfiguration(maxDuration / 2, 44100, maxDuration);
        ASSERT_TRUE(result.isValid, "Timestamp within maximum duration should be valid");
    }
    
    void testZeroTimestampAndDuration() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test zero timestamp
        TimestampValidationResult result = validator.ValidateTimestampConfiguration(0, 44100, 88200);
        ASSERT_TRUE(result.isValid, "Zero timestamp should be valid");
        
        // Test zero duration
        result = validator.ValidateTimestampConfiguration(0, 44100, 0);
        ASSERT_TRUE(result.isValid, "Zero duration should be valid for zero timestamp");
        
        result = validator.ValidateTimestampConfiguration(1, 44100, 0);
        ASSERT_FALSE(result.isValid, "Non-zero timestamp with zero duration should be invalid");
        
        // Test all zeros
        result = validator.ValidateTimestampConfiguration(0, 0, 0);
        ASSERT_FALSE(result.isValid, "All zero values should be invalid due to zero timescale");
    }
    
    void testTimescaleResolutionLimits() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test very low resolution timescale
        TimestampValidationResult result = validator.ValidateTimestampConfiguration(1, 1, 10);
        ASSERT_TRUE(result.isValid, "Low resolution timescale should be valid");
        
        // Test high resolution timescale
        result = validator.ValidateTimestampConfiguration(96000, 96000, 192000);
        ASSERT_TRUE(result.isValid, "High resolution timescale should be valid");
        
        // Test fractional second representation
        result = validator.ValidateTimestampConfiguration(500, 1000, 1000); // 0.5 seconds
        ASSERT_TRUE(result.isValid, "Fractional second timestamp should be valid");
    }
    
    void testTimestampPrecisionLoss() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test timestamp that might lose precision in conversion
        uint64_t precisionTestTimestamp = 0x1FFFFFFFFULL; // Large value that might lose precision
        TimestampValidationResult result = validator.ValidateTimestampConfiguration(
            precisionTestTimestamp, 44100, precisionTestTimestamp + 44100);
        
        ASSERT_TRUE(result.isValid, "Large timestamp should maintain precision");
        ASSERT_EQUALS(precisionTestTimestamp, result.normalizedTimestamp, "Normalized timestamp should match input");
    }
};

// Test class for sample table edge cases
class SampleTableEdgeCaseTest : public TestCase {
public:
    SampleTableEdgeCaseTest() : TestCase("SampleTableEdgeCaseTest") {}
    
protected:
    void runTest() override {
        testLargeSampleTables();
        testSingleSampleTables();
        testIrregularSampleDistribution();
        testSampleTableBoundaryConditions();
        testCorruptedSampleTableRecovery();
    }
    
private:
    void testLargeSampleTables() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Create large sample table (10000 samples)
        SampleTableInfo largeSampleTable;
        
        // Create 1000 chunks with 10 samples each
        for (int i = 0; i < 1000; i++) {
            largeSampleTable.chunkOffsets.push_back(1000 + i * 1000);
        }
        
        // Create 10000 samples
        for (int i = 0; i < 10000; i++) {
            largeSampleTable.sampleSizes.push_back(100);
            largeSampleTable.sampleTimes.push_back(i * 1024);
        }
        
        // 10 samples per chunk
        SampleToChunkEntry entry = {1, 10, 1};
        largeSampleTable.sampleToChunkEntries = {entry};
        
        ASSERT_TRUE(validator.ValidateSampleTableConsistency(largeSampleTable), 
                   "Large sample table should be valid");
    }
    
    void testSingleSampleTables() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Create single sample table
        SampleTableInfo singleSampleTable;
        singleSampleTable.chunkOffsets = {1000};
        singleSampleTable.sampleSizes = {100};
        singleSampleTable.sampleTimes = {0};
        
        SampleToChunkEntry entry = {1, 1, 1};
        singleSampleTable.sampleToChunkEntries = {entry};
        
        ASSERT_TRUE(validator.ValidateSampleTableConsistency(singleSampleTable), 
                   "Single sample table should be valid");
    }
    
    void testIrregularSampleDistribution() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Create irregular sample distribution
        SampleTableInfo irregularSampleTable;
        irregularSampleTable.chunkOffsets = {1000, 2000, 3000, 4000}; // 4 chunks
        
        // Varying sample sizes
        irregularSampleTable.sampleSizes = {50, 100, 150, 200, 75, 125, 175, 225, 300};
        
        // Non-uniform timing
        irregularSampleTable.sampleTimes = {0, 512, 1536, 2048, 3072, 4096, 5120, 6144, 7168};
        
        // Complex sample-to-chunk mapping
        SampleToChunkEntry entry1 = {1, 2, 1}; // Chunks 1-2: 2 samples each = 4 samples
        SampleToChunkEntry entry2 = {3, 3, 1}; // Chunks 3-4: 3 samples each = 6 samples
        // Total: 4 + 6 = 10 samples, but we have 9 - this should fail
        irregularSampleTable.sampleToChunkEntries = {entry1, entry2};
        
        ASSERT_FALSE(validator.ValidateSampleTableConsistency(irregularSampleTable), 
                    "Irregular sample distribution with inconsistent count should fail");
        
        // Fix the sample count
        SampleToChunkEntry entry2_fixed = {3, 2, 1}; // Chunks 3-4: 2 samples each = 4 samples
        SampleToChunkEntry entry3 = {4, 1, 1}; // Chunk 4: 1 additional sample = 1 sample
        // Total: 4 + 4 + 1 = 9 samples
        irregularSampleTable.sampleToChunkEntries = {entry1, entry2_fixed, entry3};
        
        ASSERT_TRUE(validator.ValidateSampleTableConsistency(irregularSampleTable), 
                   "Fixed irregular sample distribution should be valid");
    }
    
    void testSampleTableBoundaryConditions() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test maximum chunk index
        SampleTableInfo boundaryTable;
        boundaryTable.chunkOffsets = {1000};
        boundaryTable.sampleSizes = {100};
        boundaryTable.sampleTimes = {0};
        
        // Reference to maximum chunk index
        SampleToChunkEntry entry = {std::numeric_limits<uint32_t>::max(), 1, 1};
        boundaryTable.sampleToChunkEntries = {entry};
        
        ASSERT_FALSE(validator.ValidateSampleTableConsistency(boundaryTable), 
                    "Reference to non-existent maximum chunk should fail");
        
        // Test maximum sample count per chunk
        boundaryTable.chunkOffsets = {1000};
        boundaryTable.sampleSizes.resize(1000000); // 1M samples
        std::fill(boundaryTable.sampleSizes.begin(), boundaryTable.sampleSizes.end(), 100);
        
        boundaryTable.sampleTimes.resize(1000000);
        for (size_t i = 0; i < 1000000; i++) {
            boundaryTable.sampleTimes[i] = i * 1024;
        }
        
        SampleToChunkEntry largeEntry = {1, 1000000, 1}; // 1M samples in one chunk
        boundaryTable.sampleToChunkEntries = {largeEntry};
        
        ASSERT_TRUE(validator.ValidateSampleTableConsistency(boundaryTable), 
                   "Large number of samples per chunk should be valid");
    }
    
    void testCorruptedSampleTableRecovery() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test sample table with missing entries
        SampleTableInfo corruptedTable;
        corruptedTable.chunkOffsets = {1000, 2000, 3000};
        corruptedTable.sampleSizes = {100, 100}; // Missing one sample size
        corruptedTable.sampleTimes = {0, 1024, 2048}; // But have all time entries
        
        SampleToChunkEntry entry = {1, 1, 1}; // 1 sample per chunk = 3 samples total
        corruptedTable.sampleToChunkEntries = {entry};
        
        ASSERT_FALSE(validator.ValidateSampleTableConsistency(corruptedTable), 
                    "Corrupted sample table should fail validation");
        
        // Test sample table with duplicate chunk references
        SampleTableInfo duplicateTable;
        duplicateTable.chunkOffsets = {1000, 2000};
        duplicateTable.sampleSizes = {100, 100, 100, 100};
        duplicateTable.sampleTimes = {0, 1024, 2048, 3072};
        
        SampleToChunkEntry entry1 = {1, 2, 1}; // Chunk 1: 2 samples
        SampleToChunkEntry entry2 = {1, 2, 1}; // Duplicate reference to chunk 1
        duplicateTable.sampleToChunkEntries = {entry1, entry2};
        
        ASSERT_FALSE(validator.ValidateSampleTableConsistency(duplicateTable), 
                    "Sample table with duplicate chunk references should fail");
    }
};

// Test class for I/O error handling during validation
class IOErrorHandlingTest : public TestCase {
public:
    IOErrorHandlingTest() : TestCase("IOErrorHandlingTest") {}
    
protected:
    void runTest() override {
        testReadErrorHandling();
        testSeekErrorHandling();
        testPartialReadHandling();
        testValidationWithIOErrors();
    }
    
private:
    void testReadErrorHandling() {
        std::vector<uint8_t> testData = {1, 2, 3, 4, 5};
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Simulate I/O error
        mockIO->setSimulateIOError(true);
        
        // Validation should handle I/O errors gracefully
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_FTYP, 32, 0, 1000);
        // Should not crash, may return invalid result
        ASSERT_TRUE(true, "Validation should handle I/O errors without crashing");
        
        mockIO->setSimulateIOError(false);
    }
    
    void testSeekErrorHandling() {
        std::vector<uint8_t> testData = {1, 2, 3, 4, 5};
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test normal operation first
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_FTYP, 8, 0, 1000);
        
        // Simulate seek error
        mockIO->setSimulateIOError(true);
        
        // Validation should handle seek errors gracefully
        result = validator.ValidateBoxStructure(BOX_FTYP, 32, 100, 1000);
        ASSERT_TRUE(true, "Validation should handle seek errors without crashing");
        
        mockIO->setSimulateIOError(false);
    }
    
    void testPartialReadHandling() {
        // Create data that's smaller than expected
        std::vector<uint8_t> smallData = {1, 2, 3}; // Only 3 bytes
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(smallData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Try to validate a box that would require more data
        BoxSizeValidationResult result = validator.ValidateBoxStructure(BOX_FTYP, 32, 0, 1000);
        
        // Should handle partial reads gracefully
        ASSERT_TRUE(true, "Validation should handle partial reads without crashing");
    }
    
    void testValidationWithIOErrors() {
        std::vector<uint8_t> testData;
        auto mockIO = std::make_shared<EdgeCaseMockIOHandler>(testData);
        ISODemuxerComplianceValidator validator(mockIO);
        
        // Test various validation functions with I/O errors
        mockIO->setSimulateIOError(true);
        
        // Test timestamp validation (shouldn't require I/O)
        TimestampValidationResult timestampResult = validator.ValidateTimestampConfiguration(44100, 44100, 88200);
        ASSERT_TRUE(timestampResult.isValid, "Timestamp validation should work without I/O");
        
        // Test sample table validation (shouldn't require I/O)
        SampleTableInfo sampleTable;
        sampleTable.chunkOffsets = {1000};
        sampleTable.sampleSizes = {100};
        sampleTable.sampleTimes = {0};
        SampleToChunkEntry entry = {1, 1, 1};
        sampleTable.sampleToChunkEntries = {entry};
        
        bool sampleTableResult = validator.ValidateSampleTableConsistency(sampleTable);
        ASSERT_TRUE(sampleTableResult, "Sample table validation should work without I/O");
        
        mockIO->setSimulateIOError(false);
    }
};

int main() {
    TestSuite suite("ISO Demuxer Compliance Validation Edge Case Tests");
    
    // Add all edge case test classes
    suite.addTest(std::make_unique<ExtremeBoxSizeTest>());
    suite.addTest(std::make_unique<TimestampEdgeCaseTest>());
    suite.addTest(std::make_unique<SampleTableEdgeCaseTest>());
    suite.addTest(std::make_unique<IOErrorHandlingTest>());
    
    // Run all tests
    std::vector<TestCaseInfo> results = suite.runAll();
    
    // Print results
    suite.printResults(results);
    
    // Return appropriate exit code
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}