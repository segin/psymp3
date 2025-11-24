/*
 * test_mulaw_alaw_codec_selection.cpp - Unit tests for μ-law/A-law codec selection
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

// Minimal StreamInfo structure for testing
struct StreamInfo {
    std::string codec_type;
    std::string codec_name;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
};

// Mock codec classes that replicate the actual canDecode logic
class MuLawCodec {
public:
    explicit MuLawCodec(const StreamInfo& stream_info) {}
    
    bool canDecode(const StreamInfo& stream_info) const {
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

class ALawCodec {
public:
    explicit ALawCodec(const StreamInfo& stream_info) {}
    
    bool canDecode(const StreamInfo& stream_info) const {
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

// Test counter
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

void assert_equals(const std::string& expected, const std::string& actual, const std::string& message) {
    test_count++;
    if (expected == actual) {
        passed_count++;
        std::cout << "✓ PASS: " << message << std::endl;
    } else {
        failed_count++;
        std::cout << "✗ FAIL: " << message << " - Expected: '" << expected 
                  << "', Got: '" << actual << "'" << std::endl;
    }
}

void print_test_section(const std::string& section_name) {
    std::cout << "\n" << section_name << std::endl;
    std::cout << std::string(section_name.length(), '=') << std::endl;
}

void print_results() {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "Test Results Summary" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "Total tests: " << test_count << std::endl;
    std::cout << "Passed: " << passed_count << std::endl;
    std::cout << "Failed: " << failed_count << std::endl;
    
    if (failed_count == 0) {
        std::cout << "\n✓ All tests PASSED!" << std::endl;
    } else {
        std::cout << "\n✗ " << failed_count << " tests FAILED!" << std::endl;
    }
}

// Test 1: MuLawCodec canDecode with valid codec names
void test_mulaw_codec_valid_names() {
    print_test_section("Test 1: MuLawCodec canDecode with valid codec names");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(stream_info);
    
    // Test primary identifier
    stream_info.codec_name = "mulaw";
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 'mulaw' codec name");
    
    // Test alternative identifier
    stream_info.codec_name = "pcm_mulaw";
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 'pcm_mulaw' codec name");
    
    // Test ITU-T identifier
    stream_info.codec_name = "g711_mulaw";
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 'g711_mulaw' codec name");
}

// Test 2: ALawCodec canDecode with valid codec names
void test_alaw_codec_valid_names() {
    print_test_section("Test 2: ALawCodec canDecode with valid codec names");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    ALawCodec alaw_codec(stream_info);
    
    // Test primary identifier
    stream_info.codec_name = "alaw";
    assert_true(alaw_codec.canDecode(stream_info), 
                "ALawCodec should accept 'alaw' codec name");
    
    // Test alternative identifier
    stream_info.codec_name = "pcm_alaw";
    assert_true(alaw_codec.canDecode(stream_info), 
                "ALawCodec should accept 'pcm_alaw' codec name");
    
    // Test ITU-T identifier
    stream_info.codec_name = "g711_alaw";
    assert_true(alaw_codec.canDecode(stream_info), 
                "ALawCodec should accept 'g711_alaw' codec name");
}

// Test 3: MuLawCodec rejects A-law formats
void test_mulaw_rejects_alaw() {
    print_test_section("Test 3: MuLawCodec rejects A-law formats");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(stream_info);
    
    // Test rejection of A-law primary identifier
    stream_info.codec_name = "alaw";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 'alaw' codec name");
    
    // Test rejection of A-law alternative identifier
    stream_info.codec_name = "pcm_alaw";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 'pcm_alaw' codec name");
    
    // Test rejection of A-law ITU-T identifier
    stream_info.codec_name = "g711_alaw";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 'g711_alaw' codec name");
}

// Test 4: ALawCodec rejects μ-law formats
void test_alaw_rejects_mulaw() {
    print_test_section("Test 4: ALawCodec rejects μ-law formats");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    ALawCodec alaw_codec(stream_info);
    
    // Test rejection of μ-law primary identifier
    stream_info.codec_name = "mulaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 'mulaw' codec name");
    
    // Test rejection of μ-law alternative identifier
    stream_info.codec_name = "pcm_mulaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 'pcm_mulaw' codec name");
    
    // Test rejection of μ-law ITU-T identifier
    stream_info.codec_name = "g711_mulaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 'g711_mulaw' codec name");
}

// Test 5: Both codecs reject incompatible formats
void test_reject_incompatible_formats() {
    print_test_section("Test 5: Both codecs reject incompatible formats");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(stream_info);
    ALawCodec alaw_codec(stream_info);
    
    // Test rejection of MP3
    stream_info.codec_name = "mp3";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 'mp3' codec name");
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 'mp3' codec name");
    
    // Test rejection of Vorbis
    stream_info.codec_name = "vorbis";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 'vorbis' codec name");
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 'vorbis' codec name");
    
    // Test rejection of generic PCM
    stream_info.codec_name = "pcm";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 'pcm' codec name");
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 'pcm' codec name");
    
    // Test rejection of FLAC
    stream_info.codec_name = "flac";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 'flac' codec name");
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 'flac' codec name");
    
    // Test rejection of Opus
    stream_info.codec_name = "opus";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 'opus' codec name");
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 'opus' codec name");
}

// Test 6: Both codecs reject non-audio stream types
void test_reject_non_audio_types() {
    print_test_section("Test 6: Both codecs reject non-audio stream types");
    
    StreamInfo stream_info;
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(stream_info);
    ALawCodec alaw_codec(stream_info);
    
    // Test rejection of video streams
    stream_info.codec_type = "video";
    stream_info.codec_name = "mulaw";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject video streams");
    
    stream_info.codec_name = "alaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject video streams");
    
    // Test rejection of subtitle streams
    stream_info.codec_type = "subtitle";
    stream_info.codec_name = "mulaw";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject subtitle streams");
    
    stream_info.codec_name = "alaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject subtitle streams");
    
    // Test rejection of empty codec type
    stream_info.codec_type = "";
    stream_info.codec_name = "mulaw";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject empty codec type");
    
    stream_info.codec_name = "alaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject empty codec type");
}

// Test 7: Codec name methods return correct identifiers
void test_codec_name_methods() {
    print_test_section("Test 7: Codec name methods return correct identifiers");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.codec_name = "mulaw";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    
    MuLawCodec mulaw_codec(stream_info);
    assert_equals("mulaw", mulaw_codec.getCodecName(), 
                  "MuLawCodec.getCodecName() should return 'mulaw'");
    
    stream_info.codec_name = "alaw";
    ALawCodec alaw_codec(stream_info);
    assert_equals("alaw", alaw_codec.getCodecName(), 
                  "ALawCodec.getCodecName() should return 'alaw'");
}

// Test 8: Parameter validation - bits per sample
void test_parameter_validation_bits_per_sample() {
    print_test_section("Test 8: Parameter validation - bits per sample");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    
    MuLawCodec mulaw_codec(stream_info);
    ALawCodec alaw_codec(stream_info);
    
    // Test valid 8-bit samples
    stream_info.codec_name = "mulaw";
    stream_info.bits_per_sample = 8;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 8 bits per sample");
    
    stream_info.codec_name = "alaw";
    assert_true(alaw_codec.canDecode(stream_info), 
                "ALawCodec should accept 8 bits per sample");
    
    // Test rejection of 16-bit samples
    stream_info.codec_name = "mulaw";
    stream_info.bits_per_sample = 16;
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 16 bits per sample");
    
    stream_info.codec_name = "alaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 16 bits per sample");
    
    // Test rejection of 24-bit samples
    stream_info.codec_name = "mulaw";
    stream_info.bits_per_sample = 24;
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 24 bits per sample");
    
    stream_info.codec_name = "alaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 24 bits per sample");
}

// Test 9: Parameter validation - sample rates
void test_parameter_validation_sample_rates() {
    print_test_section("Test 9: Parameter validation - sample rates");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.codec_name = "mulaw";
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(stream_info);
    
    // Test valid telephony sample rate
    stream_info.sample_rate = 8000;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 8 kHz sample rate");
    
    // Test valid wideband sample rate
    stream_info.sample_rate = 16000;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 16 kHz sample rate");
    
    // Test valid super-wideband sample rate
    stream_info.sample_rate = 32000;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 32 kHz sample rate");
    
    // Test valid CD quality sample rate
    stream_info.sample_rate = 44100;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 44.1 kHz sample rate");
    
    // Test valid professional audio sample rate
    stream_info.sample_rate = 48000;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 48 kHz sample rate");
    
    // Test that 0 Hz is allowed (unspecified, will use default 8 kHz)
    stream_info.sample_rate = 0;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 0 Hz sample rate (unspecified, will use default)");
    
    // Test rejection of extremely high sample rate
    stream_info.sample_rate = 300000;
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 300 kHz sample rate");
}

// Test 10: Parameter validation - channel counts
void test_parameter_validation_channels() {
    print_test_section("Test 10: Parameter validation - channel counts");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.codec_name = "mulaw";
    stream_info.sample_rate = 8000;
    stream_info.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(stream_info);
    ALawCodec alaw_codec(stream_info);
    
    // Test valid mono
    stream_info.channels = 1;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept mono (1 channel)");
    
    stream_info.codec_name = "alaw";
    assert_true(alaw_codec.canDecode(stream_info), 
                "ALawCodec should accept mono (1 channel)");
    
    // Test valid stereo
    stream_info.codec_name = "mulaw";
    stream_info.channels = 2;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept stereo (2 channels)");
    
    stream_info.codec_name = "alaw";
    assert_true(alaw_codec.canDecode(stream_info), 
                "ALawCodec should accept stereo (2 channels)");
    
    // Test rejection of 3 channels
    stream_info.codec_name = "mulaw";
    stream_info.channels = 3;
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 3 channels");
    
    stream_info.codec_name = "alaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 3 channels");
    
    // Test rejection of 6 channels (surround)
    stream_info.codec_name = "mulaw";
    stream_info.channels = 6;
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject 6 channels (surround)");
    
    stream_info.codec_name = "alaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject 6 channels (surround)");
    
    // Test that 0 channels is allowed (will be set to default during initialization)
    stream_info.codec_name = "mulaw";
    stream_info.channels = 0;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept 0 channels (unspecified, will use default)");
    
    stream_info.codec_name = "alaw";
    assert_true(alaw_codec.canDecode(stream_info), 
                "ALawCodec should accept 0 channels (unspecified, will use default)");
}

// Test 11: Edge cases - empty codec name
void test_edge_case_empty_codec_name() {
    print_test_section("Test 11: Edge cases - empty codec name");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.codec_name = "";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(stream_info);
    ALawCodec alaw_codec(stream_info);
    
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject empty codec name");
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should reject empty codec name");
}

// Test 12: Edge cases - case sensitivity
void test_edge_case_case_sensitivity() {
    print_test_section("Test 12: Edge cases - case sensitivity");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(stream_info);
    ALawCodec alaw_codec(stream_info);
    
    // Test uppercase μ-law
    stream_info.codec_name = "MULAW";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should be case-sensitive - reject 'MULAW'");
    
    // Test uppercase A-law
    stream_info.codec_name = "ALAW";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should be case-sensitive - reject 'ALAW'");
    
    // Test mixed case
    stream_info.codec_name = "MuLaw";
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should be case-sensitive - reject 'MuLaw'");
    
    stream_info.codec_name = "ALaw";
    assert_false(alaw_codec.canDecode(stream_info), 
                 "ALawCodec should be case-sensitive - reject 'ALaw'");
}

// Test 13: Edge cases - boundary sample rates
void test_edge_case_boundary_sample_rates() {
    print_test_section("Test 13: Edge cases - boundary sample rates");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.codec_name = "mulaw";
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(stream_info);
    
    // Test minimum valid sample rate
    stream_info.sample_rate = 1;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept minimum valid sample rate (1 Hz)");
    
    // Test maximum valid sample rate
    stream_info.sample_rate = 192000;
    assert_true(mulaw_codec.canDecode(stream_info), 
                "MuLawCodec should accept maximum valid sample rate (192 kHz)");
    
    // Test just over maximum
    stream_info.sample_rate = 192001;
    assert_false(mulaw_codec.canDecode(stream_info), 
                 "MuLawCodec should reject sample rate just over maximum");
}

// Test 14: MediaFactory codec selection (simulated)
void test_media_factory_codec_selection() {
    print_test_section("Test 14: MediaFactory codec selection (simulated)");
    
    // Simulate MediaFactory selecting correct codec based on StreamInfo
    StreamInfo mulaw_stream;
    mulaw_stream.codec_type = "audio";
    mulaw_stream.codec_name = "mulaw";
    mulaw_stream.sample_rate = 8000;
    mulaw_stream.channels = 1;
    mulaw_stream.bits_per_sample = 8;
    
    StreamInfo alaw_stream;
    alaw_stream.codec_type = "audio";
    alaw_stream.codec_name = "alaw";
    alaw_stream.sample_rate = 8000;
    alaw_stream.channels = 1;
    alaw_stream.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(mulaw_stream);
    ALawCodec alaw_codec(alaw_stream);
    
    // Test that MuLawCodec accepts μ-law streams
    assert_true(mulaw_codec.canDecode(mulaw_stream), 
                "MediaFactory: MuLawCodec should accept μ-law streams");
    
    // Test that ALawCodec accepts A-law streams
    assert_true(alaw_codec.canDecode(alaw_stream), 
                "MediaFactory: ALawCodec should accept A-law streams");
    
    // Test that MuLawCodec rejects A-law streams
    assert_false(mulaw_codec.canDecode(alaw_stream), 
                 "MediaFactory: MuLawCodec should reject A-law streams");
    
    // Test that ALawCodec rejects μ-law streams
    assert_false(alaw_codec.canDecode(mulaw_stream), 
                 "MediaFactory: ALawCodec should reject μ-law streams");
}

// Test 15: Multiple codec name variants
void test_multiple_codec_name_variants() {
    print_test_section("Test 15: Multiple codec name variants");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    MuLawCodec mulaw_codec(stream_info);
    ALawCodec alaw_codec(stream_info);
    
    // Test all μ-law variants
    std::vector<std::string> mulaw_variants = {"mulaw", "pcm_mulaw", "g711_mulaw"};
    for (const auto& variant : mulaw_variants) {
        stream_info.codec_name = variant;
        assert_true(mulaw_codec.canDecode(stream_info), 
                    "MuLawCodec should accept '" + variant + "' variant");
    }
    
    // Test all A-law variants
    std::vector<std::string> alaw_variants = {"alaw", "pcm_alaw", "g711_alaw"};
    for (const auto& variant : alaw_variants) {
        stream_info.codec_name = variant;
        assert_true(alaw_codec.canDecode(stream_info), 
                    "ALawCodec should accept '" + variant + "' variant");
    }
}

int main() {
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "μ-law/A-law Codec Selection Unit Tests" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    // Run all tests
    test_mulaw_codec_valid_names();
    test_alaw_codec_valid_names();
    test_mulaw_rejects_alaw();
    test_alaw_rejects_mulaw();
    test_reject_incompatible_formats();
    test_reject_non_audio_types();
    test_codec_name_methods();
    test_parameter_validation_bits_per_sample();
    test_parameter_validation_sample_rates();
    test_parameter_validation_channels();
    test_edge_case_empty_codec_name();
    test_edge_case_case_sensitivity();
    test_edge_case_boundary_sample_rates();
    test_media_factory_codec_selection();
    test_multiple_codec_name_variants();
    
    print_results();
    
    return failed_count;
}
