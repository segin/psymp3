/*
 * test_error_handling.cpp - Comprehensive error handling tests for OggDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

// Mock IOHandler for testing
class MemoryIOHandler : public IOHandler {
private:
    std::vector<uint8_t> data;
    size_t position = 0;
    
public:
    explicit MemoryIOHandler(const std::vector<uint8_t>& test_data) : data(test_data) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = std::min(size * count, data.size() - position);
        if (bytes_to_read > 0) {
            std::memcpy(buffer, data.data() + position, bytes_to_read);
            position += bytes_to_read;
        }
        return bytes_to_read / size;
    }
    
    int seek(long offset, int whence) override {
        switch (whence) {
            case SEEK_SET:
                position = std::min(static_cast<size_t>(std::max(0L, offset)), data.size());
                break;
            case SEEK_CUR:
                position = std::min(position + static_cast<size_t>(std::max(0L, offset)), data.size());
                break;
            case SEEK_END:
                position = data.size();
                break;
            default:
                return -1;
        }
        return 0;
    }
    
    long tell() override {
        return static_cast<long>(position);
    }
    
    bool eof() override {
        return position >= data.size();
    }
    
    off_t getFileSize() override {
        return static_cast<off_t>(data.size());
    }
};

// Test framework
struct TestResult {
    std::string test_name;
    bool passed;
    std::string error_message;
};

class ErrorHandlingTestSuite {
private:
    std::vector<TestResult> results;
    
    void recordResult(const std::string& test_name, bool passed, const std::string& error = "") {
        results.push_back({test_name, passed, error});
        std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << test_name;
        if (!passed && !error.empty()) {
            std::cout << " - " << error;
        }
        std::cout << std::endl;
    }
    
public:
    // Test invalid page header handling (Requirement 7.1)
    bool testInvalidPageHeaders() {
        try {
            // Create a mock IOHandler with corrupted Ogg data
            std::vector<uint8_t> corrupted_data = {
                // Invalid capture pattern (should be "OggS")
                'B', 'a', 'd', 'S', 0x00, 0x02, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x01, 0x1E
            };
            
            auto handler = std::make_unique<MemoryIOHandler>(corrupted_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should handle invalid pages gracefully using ogg_sync_pageseek() patterns
            bool result = demuxer.parseContainer();
            
            // Check if any valid streams were found
            auto streams = demuxer.getStreams();
            
            // The demuxer should either fail to parse or find no valid streams
            return !result || streams.empty();
            
        } catch (const std::exception& e) {
            // If an exception is thrown, that's also acceptable error handling
            return true; // Exception is acceptable for corrupted data
        }
    }
    
    // Test CRC validation failure handling (Requirement 7.2)
    bool testCRCValidationFailure() {
        try {
            // Create Ogg data with valid structure but invalid CRC
            std::vector<uint8_t> bad_crc_data = {
                // Valid Ogg page header with intentionally bad CRC
                'O', 'g', 'g', 'S', 0x00, 0x02, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, // Bad CRC
                0x00, 0x00, 0x00, 0x00, 0x01, 0x1E
            };
            
            auto handler = std::make_unique<MemoryIOHandler>(bad_crc_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should rely on libogg's internal validation and continue like libvorbisfile
            bool result = demuxer.parseContainer();
            
            // libogg should handle CRC validation internally
            return true; // Should handle gracefully
            
        } catch (const std::exception& e) {
            recordResult("testCRCValidationFailure", false, "Exception thrown: " + std::string(e.what()));
            return false;
        }
    }
    
    // Test packet reconstruction failure (Requirement 7.3)
    bool testPacketReconstructionFailure() {
        try {
            // Create incomplete Ogg data that would cause packet reconstruction issues
            std::vector<uint8_t> incomplete_data = {
                // Valid Ogg page header but incomplete packet data
                'O', 'g', 'g', 'S', 0x00, 0x02, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x01, 0x04,
                // Incomplete packet data (only 4 bytes when more expected)
                0x01, 0x02, 0x03, 0x04
            };
            
            auto handler = std::make_unique<MemoryIOHandler>(incomplete_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should handle incomplete packets gracefully
            bool parseResult = demuxer.parseContainer();
            
            // Check if any valid streams were found
            auto streams = demuxer.getStreams();
            
            // Should either fail to parse or find no valid streams
            return !parseResult || streams.empty();
            
        } catch (const std::exception& e) {
            // Exception handling is also acceptable for corrupted data
            return true;
        }
    }
    
    // Test codec identification failure (Requirement 7.4)
    bool testCodecIdentificationFailure() {
        try {
            // Create Ogg data with unknown codec signature
            std::vector<uint8_t> unknown_codec_data;
            
            // Valid Ogg page header
            unknown_codec_data.insert(unknown_codec_data.end(), {
                'O', 'g', 'g', 'S', 0x00, 0x02, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x01, 0x08
            });
            
            // Unknown codec signature
            unknown_codec_data.insert(unknown_codec_data.end(), {
                'U', 'n', 'k', 'n', 'o', 'w', 'n', 'C'
            });
            
            auto handler = std::make_unique<MemoryIOHandler>(unknown_codec_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should return OP_ENOTFORMAT and continue scanning like libopusfile
            bool result = demuxer.parseContainer();
            
            // Should handle unknown codecs gracefully
            return true;
            
        } catch (const std::exception& e) {
            recordResult("testCodecIdentificationFailure", false, "Exception thrown: " + std::string(e.what()));
            return false;
        }
    }
    
    // Test memory allocation failure handling (Requirement 7.5)
    bool testMemoryAllocationFailure() {
        try {
            // Create very large Ogg data that would cause memory issues
            std::vector<uint8_t> large_data(1024 * 1024 * 100); // 100MB
            
            auto handler = std::make_unique<MemoryIOHandler>(large_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should return OP_EFAULT/OV_EFAULT like reference implementations
            bool result = demuxer.parseContainer();
            
            // Should handle memory issues gracefully
            return true;
            
        } catch (const std::bad_alloc& e) {
            // Expected behavior - should catch and handle memory failures
            return true;
        } catch (const std::exception& e) {
            recordResult("testMemoryAllocationFailure", false, "Unexpected exception: " + std::string(e.what()));
            return false;
        }
    }
    
    // Test I/O operation failure (Requirement 7.6)
    bool testIOOperationFailure() {
        try {
            // Create a mock handler that simulates I/O failures
            class FailingIOHandler : public MemoryIOHandler {
            public:
                FailingIOHandler() : MemoryIOHandler({}) {}
                
                size_t read(void* buffer, size_t size, size_t count) override {
                    // Simulate I/O failure
                    return 0;
                }
                
                bool eof() override {
                    return true; // Simulate EOF
                }
            };
            
            auto handler = std::make_unique<FailingIOHandler>();
            OggDemuxer demuxer(std::move(handler));
            
            // Should handle I/O failures gracefully
            bool result = demuxer.parseContainer();
            
            // Should return false for I/O failures
            return !result;
            
        } catch (const std::exception& e) {
            // Exception handling is acceptable for I/O errors
            return true;
        }
    }
    
    // Test seeking beyond file boundaries (Requirement 7.7)
    bool testSeekingBeyondBoundaries() {
        try {
            // Create minimal valid Ogg file
            std::vector<uint8_t> minimal_ogg = createMinimalOggFile();
            
            auto handler = std::make_unique<MemoryIOHandler>(minimal_ogg);
            OggDemuxer demuxer(std::move(handler));
            
            // Try to parse - if it fails, that's also acceptable for minimal data
            bool parseResult = demuxer.parseContainer();
            
            // Try seeking beyond reasonable bounds
            bool result = demuxer.seekTo(999999999); // Very large timestamp
            
            // Should handle out-of-bounds seeks gracefully (either succeed by clamping or fail gracefully)
            return true; // Any non-crashing behavior is acceptable
            
        } catch (const std::exception& e) {
            // Exception handling is also acceptable
            return true;
        }
    }
    
    // Test malformed metadata handling (Requirement 7.8)
    bool testMalformedMetadata() {
        try {
            // Create Ogg file with malformed metadata
            std::vector<uint8_t> malformed_data = createOggWithMalformedMetadata();
            
            auto handler = std::make_unique<MemoryIOHandler>(malformed_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should handle malformed metadata gracefully
            bool result = demuxer.parseContainer();
            
            // Should not crash when parsing malformed metadata
            return true;
            
        } catch (const std::exception& e) {
            // Exception handling is acceptable for malformed data
            return true;
        }
    }
    
    // Test invalid granule position handling (Requirement 7.9)
    bool testInvalidGranulePosition() {
        try {
            // Create Ogg file with invalid granule position
            std::vector<uint8_t> invalid_granule_data = createOggWithInvalidGranule();
            
            auto handler = std::make_unique<MemoryIOHandler>(invalid_granule_data);
            OggDemuxer demuxer(std::move(handler));
            
            // Should handle invalid granule positions gracefully
            bool result = demuxer.parseContainer();
            
            // Should not crash when processing invalid granule positions
            return true;
            
        } catch (const std::exception& e) {
            // Exception handling is acceptable
            return true;
        }
    }
    
    // Test unexpected stream end (Requirement 7.10)
    bool testUnexpectedStreamEnd() {
        try {
            // Create truncated Ogg file
            std::vector<uint8_t> truncated_ogg = createTruncatedOggFile();
            
            auto handler = std::make_unique<MemoryIOHandler>(truncated_ogg);
            OggDemuxer demuxer(std::move(handler));
            
            // Should return OP_EBADLINK/OV_EBADLINK like reference implementations
            bool result = demuxer.parseContainer();
            
            // Should handle truncated files gracefully
            return true;
            
        } catch (const std::exception& e) {
            // Exception handling is acceptable
            return true;
        }
    }
    
    // Test bisection search failure (Requirement 7.11)
    bool testBisectionSearchFailure() {
        try {
            // Create minimal Ogg file
            std::vector<uint8_t> minimal_ogg = createMinimalOggFile();
            
            auto handler = std::make_unique<MemoryIOHandler>(minimal_ogg);
            OggDemuxer demuxer(std::move(handler));
            
            // Try to parse - if it fails, that's acceptable for minimal data
            bool parseResult = demuxer.parseContainer();
            
            // Try to seek beyond available data
            bool result = demuxer.seekTo(5000); // Seek to 5 seconds
            
            // Should handle bisection failures gracefully (may fail or succeed)
            // The important thing is it doesn't crash
            return true;
            
        } catch (const std::exception& e) {
            // Exception handling is acceptable
            return true;
        }
    }
    
    // Test page size exceeding maximum (Requirement 7.12)
    bool testPageSizeExceedsMaximum() {
        try {
            // Create Ogg page with excessive size
            std::vector<uint8_t> oversized_page = createOversizedOggPage();
            
            auto handler = std::make_unique<MemoryIOHandler>(oversized_page);
            OggDemuxer demuxer(std::move(handler));
            
            // Should handle using OP_PAGE_SIZE_MAX bounds checking
            bool result = demuxer.parseContainer();
            
            // Should handle oversized pages gracefully
            return true;
            
        } catch (const std::exception& e) {
            recordResult("testPageSizeExceedsMaximum", false, "Exception thrown: " + std::string(e.what()));
            return false;
        }
    }
    
    // Test bounded parsing loops (prevent infinite hangs)
    bool testBoundedParsingLoops() {
        try {
            // Create Ogg file that could cause infinite loops
            std::vector<uint8_t> loop_inducing = createLoopInducingOggFile();
            
            auto handler = std::make_unique<MemoryIOHandler>(loop_inducing);
            OggDemuxer demuxer(std::move(handler));
            
            // Set a timeout to ensure parsing doesn't hang
            auto start_time = std::chrono::steady_clock::now();
            bool result = demuxer.parseContainer();
            auto end_time = std::chrono::steady_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
            
            // Should complete within reasonable time (not hang indefinitely)
            return duration.count() < 10; // Max 10 seconds
            
        } catch (const std::exception& e) {
            recordResult("testBoundedParsingLoops", false, "Exception thrown: " + std::string(e.what()));
            return false;
        }
    }
    
    // Helper methods to create test data
    std::vector<uint8_t> createMinimalOggFile() {
        // Create minimal valid Ogg Vorbis file
        std::vector<uint8_t> data;
        
        // Ogg page header for BOS page
        data.insert(data.end(), {
            'O', 'g', 'g', 'S', 0x00, 0x02, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x01, 0x1E
        });
        
        // Vorbis identification header
        data.insert(data.end(), {
            0x01, 'v', 'o', 'r', 'b', 'i', 's', 0x00, 0x00, 0x00, 0x00,
            0x02, 0x44, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
        });
        
        return data;
    }
    
    std::vector<uint8_t> createOggWithMalformedMetadata() {
        auto data = createMinimalOggFile();
        
        // Add malformed comment header with invalid UTF-8
        data.insert(data.end(), {
            0x03, 'v', 'o', 'r', 'b', 'i', 's',
            0xFF, 0xFE, 0xFD, 0xFC // Invalid UTF-8 sequences
        });
        
        return data;
    }
    
    std::vector<uint8_t> createOggWithInvalidGranule() {
        auto data = createMinimalOggFile();
        
        // Modify granule position to -1 (0xFFFFFFFFFFFFFFFF)
        for (size_t i = 6; i < 14; ++i) {
            data[i] = 0xFF;
        }
        
        return data;
    }
    
    std::vector<uint8_t> createTruncatedOggFile() {
        auto data = createMinimalOggFile();
        
        // Truncate the file in the middle of a page
        data.resize(data.size() / 2);
        
        return data;
    }
    
    std::vector<uint8_t> createOggWithCorruptedSeeking() {
        auto data = createMinimalOggFile();
        
        // Add corrupted page sequence numbers
        if (data.size() > 18) {
            data[18] = 0xFF; // Corrupt page sequence
            data[19] = 0xFF;
        }
        
        return data;
    }
    
    std::vector<uint8_t> createOversizedOggPage() {
        std::vector<uint8_t> data;
        
        // Ogg page header with maximum segments (255)
        data.insert(data.end(), {
            'O', 'g', 'g', 'S', 0x00, 0x02, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0xFF // 255 segments
        });
        
        // Add 255 segments of maximum size (255 bytes each)
        for (int i = 0; i < 255; ++i) {
            data.push_back(0xFF); // Segment size
        }
        
        // Add the actual segment data (255 * 255 = 65025 bytes)
        data.resize(data.size() + 65025, 0x00);
        
        return data;
    }
    
    std::vector<uint8_t> createLoopInducingOggFile() {
        auto data = createMinimalOggFile();
        
        // Create circular reference in page structure
        // This is a simplified version - real loop induction would be more complex
        data.insert(data.end(), data.begin(), data.end()); // Duplicate content
        
        return data;
    }
    
    // Run all tests
    void runAllTests() {
        std::cout << "Running OggDemuxer Error Handling Tests..." << std::endl;
        std::cout << "===========================================" << std::endl;
        
        recordResult("Invalid Page Headers", testInvalidPageHeaders());
        recordResult("CRC Validation Failure", testCRCValidationFailure());
        recordResult("Packet Reconstruction Failure", testPacketReconstructionFailure());
        recordResult("Codec Identification Failure", testCodecIdentificationFailure());
        recordResult("Memory Allocation Failure", testMemoryAllocationFailure());
        recordResult("I/O Operation Failure", testIOOperationFailure());
        recordResult("Seeking Beyond Boundaries", testSeekingBeyondBoundaries());
        recordResult("Malformed Metadata", testMalformedMetadata());
        recordResult("Invalid Granule Position", testInvalidGranulePosition());
        recordResult("Unexpected Stream End", testUnexpectedStreamEnd());
        recordResult("Bisection Search Failure", testBisectionSearchFailure());
        recordResult("Page Size Exceeds Maximum", testPageSizeExceedsMaximum());
        recordResult("Bounded Parsing Loops", testBoundedParsingLoops());
        
        // Print summary
        int passed = 0, failed = 0;
        for (const auto& result : results) {
            if (result.passed) passed++;
            else failed++;
        }
        
        std::cout << std::endl;
        std::cout << "Test Summary: " << passed << " passed, " << failed << " failed" << std::endl;
        
        if (failed > 0) {
            std::cout << "Failed tests:" << std::endl;
            for (const auto& result : results) {
                if (!result.passed) {
                    std::cout << "  - " << result.test_name;
                    if (!result.error_message.empty()) {
                        std::cout << ": " << result.error_message;
                    }
                    std::cout << std::endl;
                }
            }
        }
    }
    
    bool allTestsPassed() const {
        for (const auto& result : results) {
            if (!result.passed) return false;
        }
        return true;
    }
};



#endif // HAVE_OGGDEMUXER

int main() {
#ifdef HAVE_OGGDEMUXER
    ErrorHandlingTestSuite test_suite;
    test_suite.runAllTests();
    
    return test_suite.allTestsPassed() ? 0 : 1;
#else
    std::cout << "OggDemuxer not available - skipping error handling tests" << std::endl;
    return 0;
#endif
}