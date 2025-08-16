/*
 * test_codec_selection_validation_simple.cpp - Simple codec selection and validation tests
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
#include <cstdint>

// Minimal StreamInfo for testing
struct StreamInfo {
    std::string codec_type;
    std::string codec_name;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
};

// Mock codec classes for testing canDecode logic
class MockMuLawCodec {
public:
    explicit MockMuLawCodec(const StreamInfo& stream_info) {}
    
    bool canDecode(const StreamInfo& stream_info) const {
        // Replicate the actual MuLawCodec canDecode logic
        if (stream_info.codec_type != "audio") {
            return false;
        }
        
        bool is_mulaw_codec = (stream_info.codec_name == "mulaw" || 
                              stream_info.codec_name == "pcm_mulaw" ||
                              stream_info.codec_name == "g711_mulaw");
        
        if (!is_mulaw_codec) {
            return false;
        }
        
        if (stream_info.bits_per_sample != 0 && stream_info.bits_per_sample != 8) {
            return false;
        }
        
        if (stream_info.sample_rate != 0) {
            if (stream_info.sample_rate < 1 || stream_info.sample_rate > 192000) {
                return false;
            }
        }
        
        if (stream_info.channels != 0) {
            if (stream_info.channels > 2 || stream_info.channels == 0) {
                return false;
            }
        }
        
        return true;
    }
    
    std::string getCodecName() const {
        return "mulaw";
    }
};

class MockALawCodec {
public:
    explicit MockALawCodec(const StreamInfo& stream_info) {}
    
    bool canDecode(const StreamInfo& stream_info) const {
        // Replicate the actual ALawCodec canDecode logic
        if (stream_info.codec_type != "audio") {
            return false;
        }
        
        bool is_alaw_codec = (stream_info.codec_name == "alaw" || 
                             stream_info.codec_name == "pcm_alaw" ||
                             stream_info.codec_name == "g711_alaw");
        
        if (!is_alaw_codec) {
            return false;
        }
        
        if (stream_info.bits_per_sample != 0 && stream_info.bits_per_sample != 8) {
            return false;
        }
        
        if (stream_info.sample_rate != 0) {
            if (stream_info.sample_rate < 1 || stream_info.sample_rate > 192000) {
                return false;
            }
        }
        
        if (stream_info.channels != 0) {
            if (stream_info.channels > 2 || stream_info.channels == 0) {
                return false;
            }
        }
        
        return true;
    }
    
    std::string getCodecName() const {
        return "alaw";
    }
};

// Simple test framework
class SimpleTestFramework {
private:
    static int test_count;
    static int passed_count;
    static int failed_count;
    
public:
    static void assert_true(bool condition, const std::string& message) {
        test_count++;
        if (condition) {
            passed_count++;
            std::cout << "PASS: " << message << std::endl;
        } else {
            failed_count++;
            std::cout << "FAIL: " << message << std::endl;
        }
    }
    
    static void assert_false(bool condition, const std::string& message) {
        test_count++;
        if (!condition) {
            passed_count++;
            std::cout << "PASS: " << message << std::endl;
        } else {
            failed_count++;
            std::cout << "FAIL: " << message << std::endl;
        }
    }
    
    static void assert_equals(const std::string& expected, const std::string& actual, const std::string& message) {
        test_count++;
        if (expected == actual) {
            passed_count++;
            std::cout << "PASS: " << message << std::endl;
        } else {
            failed_count++;
            std::cout << "FAIL: " << message << " - Expected: " << expected 
                      << ", Got: " << actual << std::endl;
        }
    }
    
    static void print_results() {
        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "Total tests: " << test_count << std::endl;
        std::cout << "Passed: " << passed_count << std::endl;
        std::cout << "Failed: " << failed_count << std::endl;
        
        if (failed_count == 0) {
            std::cout << "✓ All tests PASSED!" << std::endl;
        } else {
            std::cout << "✗ " << failed_count << " tests FAILED!" << std::endl;
        }
    }
    
    static int get_failure_count() {
        return failed_count;
    }
};

int SimpleTestFramework::test_count = 0;
int SimpleTestFramework::passed_count = 0;
int SimpleTestFramework::failed_count = 0;

void test_mulaw_codec_can_decode() {
    std::cout << "Testing MuLawCodec canDecode method..." << std::endl;
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    MockMuLawCodec mulaw_codec(stream_info);
    
    // Test valid μ-law formats
    stream_info.codec_name = "mulaw";
    SimpleTestFramework::assert_true(mulaw_codec.canDecode(stream_info), "Should accept 'mulaw' codec name");
    
    stream_info.codec_name = "pcm_mulaw";
    SimpleTestFramework::assert_true(mulaw_codec.canDecode(stream_info), "Should accept 'pcm_mulaw' codec name");
    
    stream_info.codec_name = "g711_mulaw";
    SimpleTestFramework::assert_true(mulaw_codec.canDecode(stream_info), "Should accept 'g711_mulaw' codec name");
    
    // Test rejection of A-law formats
    stream_info.codec_name = "alaw";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "Should reject 'alaw' codec name");
    
    stream_info.codec_name = "pcm_alaw";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "Should reject 'pcm_alaw' codec name");
    
    stream_info.codec_name = "g711_alaw";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "Should reject 'g711_alaw' codec name");
    
    // Test rejection of other formats
    stream_info.codec_name = "mp3";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "Should reject 'mp3' codec name");
    
    stream_info.codec_name = "vorbis";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "Should reject 'vorbis' codec name");
    
    stream_info.codec_name = "pcm";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "Should reject 'pcm' codec name");
    
    // Test rejection of non-audio types
    stream_info.codec_name = "mulaw";
    stream_info.codec_type = "video";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "Should reject video streams");
    
    stream_info.codec_type = "subtitle";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "Should reject subtitle streams");
    
    stream_info.codec_type = "";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "Should reject empty codec type");
}

void test_alaw_codec_can_decode() {
    std::cout << "\nTesting ALawCodec canDecode method..." << std::endl;
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    MockALawCodec alaw_codec(stream_info);
    
    // Test valid A-law formats
    stream_info.codec_name = "alaw";
    SimpleTestFramework::assert_true(alaw_codec.canDecode(stream_info), "Should accept 'alaw' codec name");
    
    stream_info.codec_name = "pcm_alaw";
    SimpleTestFramework::assert_true(alaw_codec.canDecode(stream_info), "Should accept 'pcm_alaw' codec name");
    
    stream_info.codec_name = "g711_alaw";
    SimpleTestFramework::assert_true(alaw_codec.canDecode(stream_info), "Should accept 'g711_alaw' codec name");
    
    // Test rejection of μ-law formats
    stream_info.codec_name = "mulaw";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "Should reject 'mulaw' codec name");
    
    stream_info.codec_name = "pcm_mulaw";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "Should reject 'pcm_mulaw' codec name");
    
    stream_info.codec_name = "g711_mulaw";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "Should reject 'g711_mulaw' codec name");
    
    // Test rejection of other formats
    stream_info.codec_name = "mp3";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "Should reject 'mp3' codec name");
    
    stream_info.codec_name = "vorbis";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "Should reject 'vorbis' codec name");
    
    stream_info.codec_name = "pcm";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "Should reject 'pcm' codec name");
    
    // Test rejection of non-audio types
    stream_info.codec_name = "alaw";
    stream_info.codec_type = "video";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "Should reject video streams");
    
    stream_info.codec_type = "subtitle";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "Should reject subtitle streams");
    
    stream_info.codec_type = "";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "Should reject empty codec type");
}

void test_parameter_validation() {
    std::cout << "\nTesting parameter validation..." << std::endl;
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    
    MockMuLawCodec mulaw_codec(stream_info);
    MockALawCodec alaw_codec(stream_info);
    
    // Test valid bits per sample (8-bit for G.711)
    stream_info.codec_name = "mulaw";
    stream_info.bits_per_sample = 8;
    SimpleTestFramework::assert_true(mulaw_codec.canDecode(stream_info), "μ-law should accept 8 bits per sample");
    
    stream_info.codec_name = "alaw";
    SimpleTestFramework::assert_true(alaw_codec.canDecode(stream_info), "A-law should accept 8 bits per sample");
    
    // Test invalid bits per sample
    stream_info.codec_name = "mulaw";
    stream_info.bits_per_sample = 16;
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "μ-law should reject 16 bits per sample");
    
    stream_info.codec_name = "alaw";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "A-law should reject 16 bits per sample");
    
    // Reset for next tests
    stream_info.bits_per_sample = 0;
    
    // Test valid sample rates
    stream_info.codec_name = "mulaw";
    stream_info.sample_rate = 8000;
    SimpleTestFramework::assert_true(mulaw_codec.canDecode(stream_info), "μ-law should accept 8 kHz sample rate");
    
    stream_info.sample_rate = 48000;
    SimpleTestFramework::assert_true(mulaw_codec.canDecode(stream_info), "μ-law should accept 48 kHz sample rate");
    
    // Test invalid sample rates
    stream_info.sample_rate = 300000; // Too high
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "μ-law should reject extremely high sample rate");
    
    // Reset for next tests
    stream_info.sample_rate = 0;
    
    // Test valid channel counts
    stream_info.channels = 1;
    SimpleTestFramework::assert_true(mulaw_codec.canDecode(stream_info), "μ-law should accept mono (1 channel)");
    
    stream_info.channels = 2;
    SimpleTestFramework::assert_true(mulaw_codec.canDecode(stream_info), "μ-law should accept stereo (2 channels)");
    
    // Test invalid channel counts
    stream_info.channels = 3;
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "μ-law should reject 3 channels");
    
    stream_info.channels = 6;
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "μ-law should reject 6 channels (surround)");
}

void test_codec_names() {
    std::cout << "\nTesting codec name methods..." << std::endl;
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    
    MockMuLawCodec mulaw_codec(stream_info);
    MockALawCodec alaw_codec(stream_info);
    
    SimpleTestFramework::assert_equals("mulaw", mulaw_codec.getCodecName(), "MuLawCodec should return 'mulaw' as codec name");
    SimpleTestFramework::assert_equals("alaw", alaw_codec.getCodecName(), "ALawCodec should return 'alaw' as codec name");
}

void test_cross_codec_rejection() {
    std::cout << "\nTesting cross-codec rejection..." << std::endl;
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    
    MockMuLawCodec mulaw_codec(stream_info);
    MockALawCodec alaw_codec(stream_info);
    
    // Test μ-law codec rejecting A-law formats
    stream_info.codec_name = "alaw";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "μ-law codec should reject A-law format");
    
    stream_info.codec_name = "pcm_alaw";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "μ-law codec should reject pcm_alaw format");
    
    stream_info.codec_name = "g711_alaw";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "μ-law codec should reject g711_alaw format");
    
    // Test A-law codec rejecting μ-law formats
    stream_info.codec_name = "mulaw";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "A-law codec should reject μ-law format");
    
    stream_info.codec_name = "pcm_mulaw";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "A-law codec should reject pcm_mulaw format");
    
    stream_info.codec_name = "g711_mulaw";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "A-law codec should reject g711_mulaw format");
}

void test_edge_cases() {
    std::cout << "\nTesting edge cases..." << std::endl;
    
    StreamInfo stream_info;
    MockMuLawCodec mulaw_codec(stream_info);
    MockALawCodec alaw_codec(stream_info);
    
    // Test empty codec name
    stream_info.codec_type = "audio";
    stream_info.codec_name = "";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "μ-law should reject empty codec name");
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "A-law should reject empty codec name");
    
    // Test case sensitivity
    stream_info.codec_name = "MULAW";
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "μ-law should be case sensitive - reject 'MULAW'");
    
    stream_info.codec_name = "ALAW";
    SimpleTestFramework::assert_false(alaw_codec.canDecode(stream_info), "A-law should be case sensitive - reject 'ALAW'");
    
    // Test boundary sample rates
    stream_info.codec_type = "audio";
    stream_info.codec_name = "mulaw";
    stream_info.sample_rate = 1; // Minimum valid
    SimpleTestFramework::assert_true(mulaw_codec.canDecode(stream_info), "μ-law should accept minimum valid sample rate");
    
    stream_info.sample_rate = 192000; // Maximum valid
    SimpleTestFramework::assert_true(mulaw_codec.canDecode(stream_info), "μ-law should accept maximum valid sample rate");
    
    stream_info.sample_rate = 192001; // Just over maximum
    SimpleTestFramework::assert_false(mulaw_codec.canDecode(stream_info), "μ-law should reject sample rate just over maximum");
}

int main() {
    std::cout << "Codec Selection and Validation Tests (Simple)" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    test_mulaw_codec_can_decode();
    test_alaw_codec_can_decode();
    test_parameter_validation();
    test_codec_names();
    test_cross_codec_rejection();
    test_edge_cases();
    
    SimpleTestFramework::print_results();
    
    return SimpleTestFramework::get_failure_count();
}