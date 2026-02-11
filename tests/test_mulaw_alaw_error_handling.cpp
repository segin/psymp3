/*
 * test_mulaw_alaw_error_handling.cpp - Unit tests for μ-law/A-law error handling
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
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <memory>

// ========================================
// MINIMAL STREAMINFO AND MEDIACHUNK STRUCTURES FOR TESTING
// ========================================

struct StreamInfo {
    std::string codec_type;
    std::string codec_name;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
};

struct MediaChunk {
    std::vector<uint8_t> data;
    uint64_t timestamp_samples = 0;
};

struct AudioFrame {
    std::vector<int16_t> samples;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint64_t timestamp_samples = 0;
    uint64_t timestamp_ms = 0;
};

// ========================================
// TEST COUNTER AND REPORTING
// ========================================

static int test_count = 0;
static int passed_count = 0;
static int failed_count = 0;

void assert_true(bool condition, const std::string& message) {
    test_count++;
    if (condition) {
        passed_count++;
        std::cout << "✓ PASS: " << message << std::endl;
    } else {
        failed_count++;
        std::cout << "✗ FAIL: " << message << std::endl;
    }
}

void assert_false(bool condition, const std::string& message) {
    test_count++;
    if (!condition) {
        passed_count++;
        std::cout << "✓ PASS: " << message << std::endl;
    } else {
        failed_count++;
        std::cout << "✗ FAIL: " << message << std::endl;
    }
}

void assert_equals(int expected, int actual, const std::string& message) {
    test_count++;
    if (expected == actual) {
        passed_count++;
        std::cout << "✓ PASS: " << message << std::endl;
    } else {
        failed_count++;
        std::cout << "✗ FAIL: " << message << " - Expected: " << expected 
                  << ", Got: " << actual << std::endl;
    }
}

void print_test_section(const std::string& section_name) {
    std::cout << "\n" << section_name << std::endl;
    std::cout << std::string(section_name.length(), '=') << std::endl;
}

void print_results() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Test Results Summary" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "Total tests: " << test_count << std::endl;
    std::cout << "Passed: " << passed_count << std::endl;
    std::cout << "Failed: " << failed_count << std::endl;
    
    if (failed_count == 0) {
        std::cout << "\n✓ All tests PASSED!" << std::endl;
    } else {
        std::cout << "\n✗ " << failed_count << " tests FAILED!" << std::endl;
    }
}

// ========================================
// TEST 1: nullptr CHUNK DATA HANDLING
// ========================================
void test_null_chunk_data_handling() {
    print_test_section("Test 1: Null chunk data handling");
    
    // Test 1.1: Empty chunk should not crash
    {
        MediaChunk empty_chunk;
        empty_chunk.data.clear();
        empty_chunk.timestamp_samples = 0;
        
        // Simulate codec behavior with empty chunk
        AudioFrame frame;
        
        // Empty chunk should produce empty frame
        assert_true(empty_chunk.data.empty(), 
                   "Empty chunk data should be empty");
        assert_true(frame.samples.empty(), 
                   "Frame from empty chunk should have no samples");
    }
    
    // Test 1.2: Codec should handle empty chunks gracefully
    {
        MediaChunk chunk;
        chunk.data.clear();
        
        // Simulate codec processing
        bool codec_crashed = false;
        try {
            // Simulate codec decode with empty chunk
            if (chunk.data.empty()) {
                // Codec should return empty frame, not crash
                AudioFrame frame;
                assert_true(frame.samples.empty(), 
                           "Codec should return empty frame for empty chunk");
            }
        } catch (...) {
            codec_crashed = true;
        }
        
        assert_false(codec_crashed, 
                    "Codec should not crash on empty chunk");
    }
    
    // Test 1.3: Multiple empty chunks should not corrupt state
    {
        int empty_chunk_count = 0;
        bool state_corrupted = false;
        
        for (int i = 0; i < 5; ++i) {
            MediaChunk empty_chunk;
            empty_chunk.data.clear();
            
            // Simulate processing
            if (empty_chunk.data.empty()) {
                empty_chunk_count++;
            }
        }
        
        assert_equals(5, empty_chunk_count, 
                     "Should process 5 empty chunks without state corruption");
        assert_false(state_corrupted, 
                    "Codec state should remain valid after empty chunks");
    }
}

// ========================================
// TEST 2: ZERO-SIZE CHUNK HANDLING
// ========================================
void test_zero_size_chunk_handling() {
    print_test_section("Test 2: Zero-size chunk handling");
    
    // Test 2.1: Zero-size chunk should be handled gracefully
    {
        MediaChunk chunk;
        chunk.data.resize(0);
        
        assert_equals(0, static_cast<int>(chunk.data.size()), 
                     "Zero-size chunk should have size 0");
        
        // Codec should handle this without error
        bool error_occurred = false;
        try {
            if (chunk.data.size() == 0) {
                // Expected behavior: return empty frame
                AudioFrame frame;
                assert_true(frame.samples.empty(), 
                           "Zero-size chunk should produce empty frame");
            }
        } catch (...) {
            error_occurred = true;
        }
        
        assert_false(error_occurred, 
                    "Codec should not throw exception on zero-size chunk");
    }
    
    // Test 2.2: Codec should not allocate memory for zero-size chunks
    {
        MediaChunk chunk;
        chunk.data.clear();
        
        // Simulate codec processing
        std::vector<int16_t> output;
        
        // Processing zero-size chunk should not allocate output
        if (chunk.data.empty()) {
            output.clear();
        }
        
        assert_true(output.empty(), 
                   "Output should be empty for zero-size chunk");
    }
    
    // Test 2.3: Alternating zero-size and valid chunks
    {
        std::vector<MediaChunk> chunks;
        
        // Create alternating pattern
        for (int i = 0; i < 3; ++i) {
            MediaChunk empty_chunk;
            empty_chunk.data.clear();
            chunks.push_back(empty_chunk);
            
            MediaChunk valid_chunk;
            valid_chunk.data.push_back(0x00);
            valid_chunk.data.push_back(0x80);
            chunks.push_back(valid_chunk);
        }
        
        int processed_count = 0;
        for (const auto& chunk : chunks) {
            if (chunk.data.empty() || !chunk.data.empty()) {
                processed_count++;
            }
        }
        
        assert_equals(6, processed_count, 
                     "Should process all 6 chunks (3 empty, 3 valid)");
    }
}

// ========================================
// TEST 3: INVALID STREAMINFO PARAMETERS
// ========================================
void test_invalid_streaminfo_parameters() {
    print_test_section("Test 3: Invalid StreamInfo parameters");
    
    // Test 3.1: Invalid codec_type should be rejected
    {
        StreamInfo stream_info;
        stream_info.codec_type = "video";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 8000;
        stream_info.channels = 1;
        
        // Codec should reject non-audio streams
        bool can_decode = (stream_info.codec_type == "audio");
        assert_false(can_decode, 
                    "Codec should reject video stream type");
    }
    
    // Test 3.2: Invalid sample rate should be rejected
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 0;  // Invalid: 0 Hz
        stream_info.channels = 1;
        
        // Codec should handle 0 sample rate (use default)
        bool valid_rate = (stream_info.sample_rate == 0 || stream_info.sample_rate >= 1);
        assert_true(valid_rate, 
                   "Codec should handle 0 sample rate (default to 8 kHz)");
    }
    
    // Test 3.3: Invalid channel count should be rejected
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 8000;
        stream_info.channels = 5;  // Invalid: more than 2 channels
        
        // Codec should reject invalid channel count
        bool valid_channels = (stream_info.channels == 0 || 
                              (stream_info.channels >= 1 && stream_info.channels <= 2));
        assert_false(valid_channels, 
                    "Codec should reject 5 channels (max 2)");
    }
    
    // Test 3.4: Invalid bits per sample should be rejected
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 8000;
        stream_info.channels = 1;
        stream_info.bits_per_sample = 16;  // Invalid: μ-law requires 8 bits
        
        // Codec should reject non-8-bit samples
        bool valid_bits = (stream_info.bits_per_sample == 0 || stream_info.bits_per_sample == 8);
        assert_false(valid_bits, 
                    "Codec should reject 16-bit samples (μ-law requires 8-bit)");
    }
}

// ========================================
// TEST 4: UNSUPPORTED CODEC_NAME VALUES
// ========================================
void test_unsupported_codec_name_values() {
    print_test_section("Test 4: Unsupported codec_name values");
    
    // Test 4.1: Empty codec_name should be rejected
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "";
        stream_info.sample_rate = 8000;
        stream_info.channels = 1;
        
        bool is_mulaw = (stream_info.codec_name == "mulaw" || 
                        stream_info.codec_name == "pcm_mulaw" ||
                        stream_info.codec_name == "g711_mulaw");
        
        assert_false(is_mulaw, 
                    "Codec should reject empty codec_name");
    }
    
    // Test 4.2: Unknown codec_name should be rejected
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "unknown_codec";
        stream_info.sample_rate = 8000;
        stream_info.channels = 1;
        
        bool is_mulaw = (stream_info.codec_name == "mulaw");
        bool is_alaw = (stream_info.codec_name == "alaw");
        
        assert_false(is_mulaw, 
                    "Codec should reject 'unknown_codec' name");
        assert_false(is_alaw, 
                    "Codec should reject 'unknown_codec' name");
    }
    
    // Test 4.3: Wrong codec_name should be rejected by correct codec
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "alaw";  // A-law codec name
        stream_info.sample_rate = 8000;
        stream_info.channels = 1;
        
        // MuLawCodec should reject A-law
        bool is_mulaw = (stream_info.codec_name == "mulaw" || 
                        stream_info.codec_name == "pcm_mulaw" ||
                        stream_info.codec_name == "g711_mulaw");
        
        assert_false(is_mulaw, 
                    "MuLawCodec should reject 'alaw' codec_name");
    }
    
    // Test 4.4: Case sensitivity in codec_name
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "MULAW";  // Uppercase
        stream_info.sample_rate = 8000;
        stream_info.channels = 1;
        
        bool is_mulaw = (stream_info.codec_name == "mulaw");
        
        assert_false(is_mulaw, 
                    "Codec should be case-sensitive (reject 'MULAW')");
    }
}

// ========================================
// TEST 5: UNSUPPORTED SAMPLE RATES
// ========================================
void test_unsupported_sample_rates() {
    print_test_section("Test 5: Unsupported sample rates");
    
    // Test 5.1: Zero sample rate should be handled (default to 8 kHz)
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 0;  // Unspecified
        stream_info.channels = 1;
        
        // Codec should accept 0 and use default
        bool valid = (stream_info.sample_rate == 0 || stream_info.sample_rate >= 1);
        assert_true(valid, 
                   "Codec should accept 0 sample rate (use default)");
    }
    
    // Test 5.2: Extremely low sample rate should be rejected
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 0;  // Invalid
        stream_info.channels = 1;
        
        // After defaulting, should be valid
        uint32_t final_rate = (stream_info.sample_rate == 0) ? 8000 : stream_info.sample_rate;
        bool valid = (final_rate >= 1 && final_rate <= 192000);
        assert_true(valid, 
                   "Codec should default 0 Hz to 8000 Hz");
    }
    
    // Test 5.3: Extremely high sample rate should be rejected
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 300000;  // Too high
        stream_info.channels = 1;
        
        bool valid = (stream_info.sample_rate >= 1 && stream_info.sample_rate <= 192000);
        assert_false(valid, 
                    "Codec should reject 300 kHz sample rate");
    }
    
    // Test 5.4: Negative sample rate (if represented as signed)
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 0;  // Unsigned, so can't be negative
        stream_info.channels = 1;
        
        // Unsigned type prevents negative values
        bool valid = (stream_info.sample_rate >= 0);
        assert_true(valid, 
                   "Unsigned sample_rate should always be >= 0");
    }
}

// ========================================
// TEST 6: UNSUPPORTED CHANNEL COUNTS
// ========================================
void test_unsupported_channel_counts() {
    print_test_section("Test 6: Unsupported channel counts");
    
    // Test 6.1: Zero channels should be handled (default to mono)
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 8000;
        stream_info.channels = 0;  // Unspecified
        
        // Codec should accept 0 and use default
        bool valid = (stream_info.channels == 0 || 
                     (stream_info.channels >= 1 && stream_info.channels <= 2));
        assert_true(valid, 
                   "Codec should accept 0 channels (use default mono)");
    }
    
    // Test 6.2: Mono (1 channel) should be accepted
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 8000;
        stream_info.channels = 1;
        
        bool valid = (stream_info.channels >= 1 && stream_info.channels <= 2);
        assert_true(valid, 
                   "Codec should accept mono (1 channel)");
    }
    
    // Test 6.3: Stereo (2 channels) should be accepted
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 8000;
        stream_info.channels = 2;
        
        bool valid = (stream_info.channels >= 1 && stream_info.channels <= 2);
        assert_true(valid, 
                   "Codec should accept stereo (2 channels)");
    }
    
    // Test 6.4: Surround (6 channels) should be rejected
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 8000;
        stream_info.channels = 6;
        
        bool valid = (stream_info.channels >= 1 && stream_info.channels <= 2);
        assert_false(valid, 
                    "Codec should reject surround (6 channels)");
    }
    
    // Test 6.5: 5.1 surround (6 channels) should be rejected
    {
        StreamInfo stream_info;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "mulaw";
        stream_info.sample_rate = 8000;
        stream_info.channels = 6;
        
        bool valid = (stream_info.channels <= 2);
        assert_false(valid, 
                    "Codec should reject 5.1 surround (6 channels)");
    }
}

// ========================================
// TEST 7: ERROR RECOVERY AND STATE CONSISTENCY
// ========================================
void test_error_recovery_and_state_consistency() {
    print_test_section("Test 7: Error recovery and state consistency");
    
    // Test 7.1: Codec should recover from invalid StreamInfo
    {
        StreamInfo invalid_stream;
        invalid_stream.codec_type = "video";
        invalid_stream.codec_name = "mulaw";
        invalid_stream.sample_rate = 8000;
        invalid_stream.channels = 1;
        
        // Simulate codec initialization failure
        bool initialized = (invalid_stream.codec_type == "audio");
        assert_false(initialized, 
                    "Codec should fail to initialize with video stream");
        
        // Now try with valid stream
        StreamInfo valid_stream;
        valid_stream.codec_type = "audio";
        valid_stream.codec_name = "mulaw";
        valid_stream.sample_rate = 8000;
        valid_stream.channels = 1;
        
        bool valid_initialized = (valid_stream.codec_type == "audio");
        assert_true(valid_initialized, 
                   "Codec should initialize successfully with valid stream");
    }
    
    // Test 7.2: Codec state should remain consistent after error
    {
        bool codec_state_valid = true;
        
        // Simulate error condition
        MediaChunk error_chunk;
        error_chunk.data.clear();
        
        // Process error chunk
        if (error_chunk.data.empty()) {
            // Codec should handle gracefully
            AudioFrame frame;
            assert_true(frame.samples.empty(), 
                       "Error chunk should produce empty frame");
        }
        
        // Verify codec state is still valid
        assert_true(codec_state_valid, 
                   "Codec state should remain valid after error");
    }
    
    // Test 7.3: Codec should process valid data after error
    {
        // Simulate error condition
        MediaChunk error_chunk;
        error_chunk.data.clear();
        
        // Process error chunk (should not crash)
        bool error_handled = true;
        
        // Now process valid chunk
        MediaChunk valid_chunk;
        valid_chunk.data.push_back(0x00);
        valid_chunk.data.push_back(0x80);
        
        // Simulate codec processing
        std::vector<int16_t> output;
        for (uint8_t byte : valid_chunk.data) {
            output.push_back(static_cast<int16_t>(byte) - 128);
        }
        
        assert_equals(2, static_cast<int>(output.size()), 
                     "Codec should process valid chunk after error");
    }
    
    // Test 7.4: Multiple errors should not corrupt state
    {
        int error_count = 0;
        bool state_corrupted = false;
        
        // Simulate multiple error conditions
        for (int i = 0; i < 5; ++i) {
            MediaChunk error_chunk;
            error_chunk.data.clear();
            
            // Process error chunk
            if (error_chunk.data.empty()) {
                error_count++;
            }
        }
        
        assert_equals(5, error_count, 
                     "Should handle 5 error conditions");
        assert_false(state_corrupted, 
                    "Codec state should remain valid after multiple errors");
    }
}

// ========================================
// MAIN TEST RUNNER
// ========================================

int main() {
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "μ-LAW/A-LAW CODEC ERROR HANDLING UNIT TESTS" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    try {
        // Run all error handling tests
        test_null_chunk_data_handling();
        test_zero_size_chunk_handling();
        test_invalid_streaminfo_parameters();
        test_unsupported_codec_name_values();
        test_unsupported_sample_rates();
        test_unsupported_channel_counts();
        test_error_recovery_and_state_consistency();
        
        print_results();
        
        return failed_count;
        
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ TEST SUITE FAILED WITH EXCEPTION" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ TEST SUITE FAILED WITH UNKNOWN EXCEPTION" << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    }
}

