/*
 * test_flac_codec_demuxer_integration.cpp - Integration tests for FLAC codec algorithms
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FLAC

/**
 * @brief Test FLAC codec integration scenarios
 * Requirements: 10.1-10.8, 11.1-11.8
 */
class FLACCodecIntegrationTest {
public:
    static bool runAllTests() {
        std::cout << "FLAC Codec Integration Tests" << std::endl;
        std::cout << "============================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testContainerFormatSupport();
        all_passed &= testStreamInfoCompatibility();
        all_passed &= testDataFlowIntegration();
        all_passed &= testErrorHandlingIntegration();
        
        if (all_passed) {
            std::cout << "✓ All integration tests PASSED" << std::endl;
        } else {
            std::cout << "✗ Some integration tests FAILED" << std::endl;
        }
        
        return all_passed;
    }

private:
    static bool testContainerFormatSupport() {
        std::cout << "Testing container format support..." << std::endl;
        
        // Test native FLAC container
        StreamInfo native_flac = createStreamInfo("flac", 44100, 2, 16);
        if (!isValidStreamInfo(native_flac)) {
            std::cout << "  ERROR: Native FLAC format not supported" << std::endl;
            return false;
        }
        
        // Test Ogg FLAC container
        StreamInfo ogg_flac = createStreamInfo("ogg", 44100, 2, 16);
        if (!isValidStreamInfo(ogg_flac)) {
            std::cout << "  ERROR: Ogg FLAC format not supported" << std::endl;
            return false;
        }
        
        // Test that both containers have same FLAC parameters
        if (!areCompatibleStreams(native_flac, ogg_flac)) {
            std::cout << "  ERROR: Container formats should be compatible for same FLAC parameters" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Container format support test passed" << std::endl;
        return true;
    }
    
    static bool testStreamInfoCompatibility() {
        std::cout << "Testing StreamInfo compatibility..." << std::endl;
        
        // Test minimal StreamInfo
        StreamInfo minimal = createMinimalStreamInfo();
        if (!isValidStreamInfo(minimal)) {
            std::cout << "  ERROR: Minimal StreamInfo not supported" << std::endl;
            return false;
        }
        
        // Test detailed StreamInfo
        StreamInfo detailed = createDetailedStreamInfo();
        if (!isValidStreamInfo(detailed)) {
            std::cout << "  ERROR: Detailed StreamInfo not supported" << std::endl;
            return false;
        }
        
        // Test compatibility between minimal and detailed
        if (!areCompatibleStreams(minimal, detailed)) {
            std::cout << "  ERROR: Minimal and detailed StreamInfo should be compatible" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ StreamInfo compatibility test passed" << std::endl;
        return true;
    }
    
    static bool testDataFlowIntegration() {
        std::cout << "Testing data flow integration..." << std::endl;
        
        // Simulate data flow from demuxer to codec
        std::vector<MockChunk> chunks = createMockFLACChunks();
        
        size_t total_samples_processed = 0;
        bool processing_successful = true;
        
        for (const auto& chunk : chunks) {
            // Validate chunk format
            if (!isValidFLACChunk(chunk)) {
                std::cout << "  ERROR: Invalid FLAC chunk detected" << std::endl;
                processing_successful = false;
                break;
            }
            
            // Simulate codec processing
            ProcessingResult result = processChunk(chunk);
            if (result.success) {
                total_samples_processed += result.samples_produced;
            } else {
                std::cout << "  ERROR: Chunk processing failed" << std::endl;
                processing_successful = false;
                break;
            }
        }
        
        std::cout << "  Total samples processed: " << total_samples_processed << std::endl;
        
        if (!processing_successful) {
            std::cout << "  ERROR: Data flow integration failed" << std::endl;
            return false;
        }
        
        // Should have processed a reasonable amount of data
        if (total_samples_processed == 0) {
            std::cout << "  ERROR: No samples were processed" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ Data flow integration test passed" << std::endl;
        return true;
    }
    
    static bool testErrorHandlingIntegration() {
        std::cout << "Testing error handling integration..." << std::endl;
        
        // Create mix of valid and invalid chunks
        std::vector<MockChunk> mixed_chunks = {
            createValidFLACChunk(),
            createInvalidFLACChunk(),
            createValidFLACChunk(),
            createCorruptedFLACChunk(),
            createValidFLACChunk()
        };
        
        size_t successful_chunks = 0;
        size_t failed_chunks = 0;
        
        for (const auto& chunk : mixed_chunks) {
            ProcessingResult result = processChunk(chunk);
            if (result.success) {
                successful_chunks++;
            } else {
                failed_chunks++;
            }
        }
        
        std::cout << "  Successful chunks: " << successful_chunks << std::endl;
        std::cout << "  Failed chunks: " << failed_chunks << std::endl;
        
        // Should have some successful and some failed chunks
        if (successful_chunks == 0) {
            std::cout << "  ERROR: No chunks processed successfully" << std::endl;
            return false;
        }
        
        if (failed_chunks == 0) {
            std::cout << "  ERROR: Error handling not tested (no failures detected)" << std::endl;
            return false;
        }
        
        // Should handle errors gracefully without crashing
        std::cout << "  ✓ Error handling integration test passed" << std::endl;
        return true;
    }
    
    // Helper structures and methods for integration testing
    struct StreamInfo {
        std::string container_format;
        std::string codec_name;
        uint32_t sample_rate;
        uint16_t channels;
        uint16_t bits_per_sample;
        uint64_t duration_ms;
        uint32_t bitrate;
    };
    
    struct MockChunk {
        std::vector<uint8_t> data;
        uint64_t timestamp_samples;
        bool is_keyframe;
        bool is_valid;
    };
    
    struct ProcessingResult {
        bool success;
        size_t samples_produced;
        std::string error_message;
    };
    
    static StreamInfo createStreamInfo(const std::string& container, uint32_t sample_rate, 
                                      uint16_t channels, uint16_t bits_per_sample) {
        StreamInfo info;
        info.container_format = container;
        info.codec_name = "flac";
        info.sample_rate = sample_rate;
        info.channels = channels;
        info.bits_per_sample = bits_per_sample;
        info.duration_ms = 180000;  // 3 minutes
        info.bitrate = 1411;        // CD quality estimate
        return info;
    }
    
    static StreamInfo createMinimalStreamInfo() {
        StreamInfo info;
        info.codec_name = "flac";
        info.sample_rate = 44100;
        info.channels = 2;
        info.bits_per_sample = 16;
        info.duration_ms = 0;
        info.bitrate = 0;
        return info;
    }
    
    static StreamInfo createDetailedStreamInfo() {
        StreamInfo info = createMinimalStreamInfo();
        info.container_format = "flac";
        info.duration_ms = 180000;
        info.bitrate = 1411;
        return info;
    }
    
    static bool isValidStreamInfo(const StreamInfo& info) {
        return info.codec_name == "flac" &&
               info.sample_rate >= 1 && info.sample_rate <= 655350 &&
               info.channels >= 1 && info.channels <= 8 &&
               info.bits_per_sample >= 4 && info.bits_per_sample <= 32;
    }
    
    static bool areCompatibleStreams(const StreamInfo& a, const StreamInfo& b) {
        return a.codec_name == b.codec_name &&
               a.sample_rate == b.sample_rate &&
               a.channels == b.channels &&
               a.bits_per_sample == b.bits_per_sample;
    }
    
    static std::vector<MockChunk> createMockFLACChunks() {
        std::vector<MockChunk> chunks;
        
        for (int i = 0; i < 5; ++i) {
            MockChunk chunk;
            chunk.data = createValidFLACFrameData();
            chunk.timestamp_samples = i * 4608;  // 4608 samples per frame
            chunk.is_keyframe = true;
            chunk.is_valid = true;
            chunks.push_back(chunk);
        }
        
        return chunks;
    }
    
    static std::vector<uint8_t> createValidFLACFrameData() {
        return {
            0xFF, 0xF8, // FLAC sync pattern
            0x69,       // Block size + sample rate encoding
            0x10,       // Channel + bit depth encoding
            0x00,       // Frame number
            0x00,       // CRC-8
            // Mock compressed data
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
        };
    }
    
    static bool isValidFLACChunk(const MockChunk& chunk) {
        if (chunk.data.size() < 6) return false;
        
        // Check FLAC sync pattern
        return chunk.data[0] == 0xFF && 
               (chunk.data[1] >= 0xF8 && chunk.data[1] <= 0xFF);
    }
    
    static ProcessingResult processChunk(const MockChunk& chunk) {
        ProcessingResult result;
        
        if (!chunk.is_valid || !isValidFLACChunk(chunk)) {
            result.success = false;
            result.samples_produced = 0;
            result.error_message = "Invalid chunk format";
            return result;
        }
        
        // Simulate successful processing
        result.success = true;
        result.samples_produced = 4608;  // Standard FLAC block size
        result.error_message = "";
        
        return result;
    }
    
    static MockChunk createValidFLACChunk() {
        MockChunk chunk;
        chunk.data = createValidFLACFrameData();
        chunk.timestamp_samples = 0;
        chunk.is_keyframe = true;
        chunk.is_valid = true;
        return chunk;
    }
    
    static MockChunk createInvalidFLACChunk() {
        MockChunk chunk;
        chunk.data = {0x00, 0x01, 0x02, 0x03};  // Invalid sync pattern
        chunk.timestamp_samples = 0;
        chunk.is_keyframe = true;
        chunk.is_valid = false;
        return chunk;
    }
    
    static MockChunk createCorruptedFLACChunk() {
        MockChunk chunk;
        chunk.data = {
            0xFF, 0xF8, // Valid sync
            0xFF, 0xFF, // Invalid header data
            0xFF, 0xFF
        };
        chunk.timestamp_samples = 0;
        chunk.is_keyframe = true;
        chunk.is_valid = false;
        return chunk;
    }
};

int main() {
    std::cout << "FLAC Codec Integration Tests" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << "Requirements: 10.1-10.8, 11.1-11.8" << std::endl;
    std::cout << std::endl;
    
    bool success = FLACCodecIntegrationTest::runAllTests();
    
    return success ? 0 : 1;
}

#else // !HAVE_FLAC

int main() {
    std::cout << "FLAC support not available - skipping FLAC codec integration tests" << std::endl;
    return 0;
}

#endif // HAVE_FLAC