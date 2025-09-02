/*
 * test_time_conversion.cpp - Unit tests for OggDemuxer time conversion functions
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include "test_framework.h"
#include <memory>
#include <vector>
#include <cmath>

/**
 * @brief Mock IOHandler for testing that doesn't require actual files
 */
class MockIOHandler : public IOHandler {
public:
    MockIOHandler() : IOHandler() {}
    
    size_t read(void* buffer, size_t size, size_t count) override { return 0; }
    int seek(off_t offset, int whence) override { return 0; }
    off_t tell() override { return 0; }
    int close() override { return 0; }
    bool eof() override { return true; }
    off_t getFileSize() override { return 0; }
};

/**
 * @brief Test fixture for time conversion tests
 */
class TimeConversionTest {
public:
    TimeConversionTest() {
        // Create a mock IOHandler for testing
        auto mock_handler = std::make_unique<MockIOHandler>();
        demuxer = std::make_unique<OggDemuxer>(std::move(mock_handler));
        
        // Set up test streams with different codec types
        setupTestStreams();
    }
    
    void setupTestStreams() {
        auto& streams = demuxer->getStreamsForTesting();
        
        // Vorbis stream (44.1kHz, stereo)
        OggStream vorbis_stream;
        vorbis_stream.serial_number = 1001;
        vorbis_stream.codec_name = "vorbis";
        vorbis_stream.codec_type = "audio";
        vorbis_stream.sample_rate = 44100;
        vorbis_stream.channels = 2;
        vorbis_stream.headers_complete = true;
        streams[1001] = vorbis_stream;
        
        // Opus stream (48kHz output, 2 channels, 312 pre-skip samples)
        OggStream opus_stream;
        opus_stream.serial_number = 1002;
        opus_stream.codec_name = "opus";
        opus_stream.codec_type = "audio";
        opus_stream.sample_rate = 48000;  // Output sample rate
        opus_stream.channels = 2;
        opus_stream.pre_skip = 312;       // Typical Opus pre-skip
        opus_stream.headers_complete = true;
        streams[1002] = opus_stream;
        
        // FLAC stream (96kHz, stereo)
        OggStream flac_stream;
        flac_stream.serial_number = 1003;
        flac_stream.codec_name = "flac";
        flac_stream.codec_type = "audio";
        flac_stream.sample_rate = 96000;
        flac_stream.channels = 2;
        flac_stream.headers_complete = true;
        streams[1003] = flac_stream;
        
        // Speex stream (16kHz, mono)
        OggStream speex_stream;
        speex_stream.serial_number = 1004;
        speex_stream.codec_name = "speex";
        speex_stream.codec_type = "audio";
        speex_stream.sample_rate = 16000;
        speex_stream.channels = 1;
        speex_stream.headers_complete = true;
        streams[1004] = speex_stream;
        
        // Unknown codec stream (22.05kHz, stereo)
        OggStream unknown_stream;
        unknown_stream.serial_number = 1005;
        unknown_stream.codec_name = "unknown";
        unknown_stream.codec_type = "audio";
        unknown_stream.sample_rate = 22050;
        unknown_stream.channels = 2;
        unknown_stream.headers_complete = true;
        streams[1005] = unknown_stream;
    }
    
    std::unique_ptr<OggDemuxer> demuxer;
};

/**
 * @brief Test Vorbis time conversion accuracy
 */
bool test_vorbis_time_conversion() {
    TimeConversionTest test;
    uint32_t vorbis_stream_id = 1001;
    
    // Test basic conversions
    struct TestCase {
        uint64_t granule;
        uint64_t expected_ms;
        const char* description;
    };
    
    TestCase test_cases[] = {
        {0, 0, "Zero granule position"},
        {44100, 1000, "1 second at 44.1kHz"},
        {88200, 2000, "2 seconds at 44.1kHz"},
        {22050, 500, "0.5 seconds at 44.1kHz"},
        {441, 10, "10ms at 44.1kHz"},
        {4410, 100, "100ms at 44.1kHz"},
        {132300, 3000, "3 seconds at 44.1kHz"},
        {1323000, 30000, "30 seconds at 44.1kHz"},
    };
    
    for (const auto& tc : test_cases) {
        uint64_t result_ms = test.demuxer->granuleToMs(tc.granule, vorbis_stream_id);
        if (result_ms != tc.expected_ms) {
            Debug::log("test", "FAIL: Vorbis granuleToMs - ", tc.description, 
                       " - granule=", tc.granule, ", expected=", tc.expected_ms, "ms, got=", result_ms, "ms");
            return false;
        }
        
        // Test reverse conversion
        uint64_t result_granule = test.demuxer->msToGranule(tc.expected_ms, vorbis_stream_id);
        if (result_granule != tc.granule) {
            Debug::log("test", "FAIL: Vorbis msToGranule - ", tc.description, 
                       " - timestamp=", tc.expected_ms, "ms, expected=", tc.granule, ", got=", result_granule);
            return false;
        }
    }
    
    Debug::log("test", "PASS: Vorbis time conversion accuracy");
    return true;
}

/**
 * @brief Test Opus time conversion with pre-skip handling
 */
bool test_opus_time_conversion() {
    TimeConversionTest test;
    uint32_t opus_stream_id = 1002;
    
    // Opus uses 48kHz granule rate regardless of output sample rate
    // Pre-skip = 312 samples at 48kHz
    
    struct TestCase {
        uint64_t granule;
        uint64_t expected_ms;
        const char* description;
    };
    
    TestCase test_cases[] = {
        {0, 0, "Zero granule position (before pre-skip)"},
        {312, 0, "Pre-skip granule position (should be 0ms)"},
        {48312, 1000, "1 second after pre-skip (48000 + 312)"},
        {96312, 2000, "2 seconds after pre-skip (96000 + 312)"},
        {24312, 500, "0.5 seconds after pre-skip (24000 + 312)"},
        {792, 10, "10ms after pre-skip (480 + 312)"},
        {5112, 100, "100ms after pre-skip (4800 + 312)"},
        {144312, 3000, "3 seconds after pre-skip (144000 + 312)"},
        {1440312, 30000, "30 seconds after pre-skip (1440000 + 312)"},
    };
    
    for (const auto& tc : test_cases) {
        uint64_t result_ms = test.demuxer->granuleToMs(tc.granule, opus_stream_id);
        if (result_ms != tc.expected_ms) {
            Debug::log("test", "FAIL: Opus granuleToMs - ", tc.description, 
                       " - granule=", tc.granule, ", expected=", tc.expected_ms, "ms, got=", result_ms, "ms");
            return false;
        }
    }
    
    // Test reverse conversion (msToGranule)
    struct ReverseTestCase {
        uint64_t timestamp_ms;
        uint64_t expected_granule;
        const char* description;
    };
    
    ReverseTestCase reverse_cases[] = {
        {0, 312, "0ms should map to pre-skip granule"},
        {1000, 48312, "1 second should map to 48000 + 312"},
        {2000, 96312, "2 seconds should map to 96000 + 312"},
        {500, 24312, "0.5 seconds should map to 24000 + 312"},
        {10, 792, "10ms should map to 480 + 312"},
        {100, 5112, "100ms should map to 4800 + 312"},
        {3000, 144312, "3 seconds should map to 144000 + 312"},
        {30000, 1440312, "30 seconds should map to 1440000 + 312"},
    };
    
    for (const auto& tc : reverse_cases) {
        uint64_t result_granule = test.demuxer->msToGranule(tc.timestamp_ms, opus_stream_id);
        if (result_granule != tc.expected_granule) {
            Debug::log("test", "FAIL: Opus msToGranule - ", tc.description, 
                       " - timestamp=", tc.timestamp_ms, "ms, expected=", tc.expected_granule, ", got=", result_granule);
            return false;
        }
    }
    
    Debug::log("test", "PASS: Opus time conversion with pre-skip handling");
    return true;
}

/**
 * @brief Test FLAC time conversion (sample-based like Vorbis)
 */
bool test_flac_time_conversion() {
    TimeConversionTest test;
    uint32_t flac_stream_id = 1003;
    
    // FLAC at 96kHz sample rate
    struct TestCase {
        uint64_t granule;
        uint64_t expected_ms;
        const char* description;
    };
    
    TestCase test_cases[] = {
        {0, 0, "Zero granule position"},
        {96000, 1000, "1 second at 96kHz"},
        {192000, 2000, "2 seconds at 96kHz"},
        {48000, 500, "0.5 seconds at 96kHz"},
        {960, 10, "10ms at 96kHz"},
        {9600, 100, "100ms at 96kHz"},
        {288000, 3000, "3 seconds at 96kHz"},
        {2880000, 30000, "30 seconds at 96kHz"},
    };
    
    for (const auto& tc : test_cases) {
        uint64_t result_ms = test.demuxer->granuleToMs(tc.granule, flac_stream_id);
        if (result_ms != tc.expected_ms) {
            Debug::log("test", "FAIL: FLAC granuleToMs - ", tc.description, 
                       " - granule=", tc.granule, ", expected=", tc.expected_ms, "ms, got=", result_ms, "ms");
            return false;
        }
        
        // Test reverse conversion
        uint64_t result_granule = test.demuxer->msToGranule(tc.expected_ms, flac_stream_id);
        if (result_granule != tc.granule) {
            Debug::log("test", "FAIL: FLAC msToGranule - ", tc.description, 
                       " - timestamp=", tc.expected_ms, "ms, expected=", tc.granule, ", got=", result_granule);
            return false;
        }
    }
    
    Debug::log("test", "PASS: FLAC time conversion accuracy");
    return true;
}

/**
 * @brief Test Speex time conversion
 */
bool test_speex_time_conversion() {
    TimeConversionTest test;
    uint32_t speex_stream_id = 1004;
    
    // Speex at 16kHz sample rate
    struct TestCase {
        uint64_t granule;
        uint64_t expected_ms;
        const char* description;
    };
    
    TestCase test_cases[] = {
        {0, 0, "Zero granule position"},
        {16000, 1000, "1 second at 16kHz"},
        {32000, 2000, "2 seconds at 16kHz"},
        {8000, 500, "0.5 seconds at 16kHz"},
        {160, 10, "10ms at 16kHz"},
        {1600, 100, "100ms at 16kHz"},
        {48000, 3000, "3 seconds at 16kHz"},
        {480000, 30000, "30 seconds at 16kHz"},
    };
    
    for (const auto& tc : test_cases) {
        uint64_t result_ms = test.demuxer->granuleToMs(tc.granule, speex_stream_id);
        if (result_ms != tc.expected_ms) {
            Debug::log("test", "FAIL: Speex granuleToMs - ", tc.description, 
                       " - granule=", tc.granule, ", expected=", tc.expected_ms, "ms, got=", result_ms, "ms");
            return false;
        }
        
        // Test reverse conversion
        uint64_t result_granule = test.demuxer->msToGranule(tc.expected_ms, speex_stream_id);
        if (result_granule != tc.granule) {
            Debug::log("test", "FAIL: Speex msToGranule - ", tc.description, 
                       " - timestamp=", tc.expected_ms, "ms, expected=", tc.granule, ", got=", result_granule);
            return false;
        }
    }
    
    Debug::log("test", "PASS: Speex time conversion accuracy");
    return true;
}

/**
 * @brief Test unknown codec time conversion (generic sample-based)
 */
bool test_unknown_codec_time_conversion() {
    TimeConversionTest test;
    uint32_t unknown_stream_id = 1005;
    
    // Unknown codec at 22.05kHz sample rate
    struct TestCase {
        uint64_t granule;
        uint64_t expected_ms;
        const char* description;
    };
    
    TestCase test_cases[] = {
        {0, 0, "Zero granule position"},
        {22050, 1000, "1 second at 22.05kHz"},
        {44100, 2000, "2 seconds at 22.05kHz"},
        {11025, 500, "0.5 seconds at 22.05kHz"},
        {23, 1, "Sub-millisecond precision (23 samples / 22050 = 1.04ms)"},
        {2205, 100, "100ms at 22.05kHz"},
        {66150, 3000, "3 seconds at 22.05kHz"},
        {661500, 30000, "30 seconds at 22.05kHz"},
    };
    
    for (const auto& tc : test_cases) {
        uint64_t result_ms = test.demuxer->granuleToMs(tc.granule, unknown_stream_id);
        if (result_ms != tc.expected_ms) {
            Debug::log("test", "FAIL: Unknown codec granuleToMs - ", tc.description, 
                       " - granule=", tc.granule, ", expected=", tc.expected_ms, "ms, got=", result_ms, "ms");
            return false;
        }
        
        // Test reverse conversion (allowing for rounding differences)
        uint64_t result_granule = test.demuxer->msToGranule(tc.expected_ms, unknown_stream_id);
        uint64_t expected_granule = tc.granule;
        
        // Allow for small rounding differences in unknown codec conversion
        if (std::abs(static_cast<int64_t>(result_granule) - static_cast<int64_t>(expected_granule)) > 1) {
            Debug::log("test", "FAIL: Unknown codec msToGranule - ", tc.description, 
                       " - timestamp=", tc.expected_ms, "ms, expected=", expected_granule, ", got=", result_granule);
            return false;
        }
    }
    
    Debug::log("test", "PASS: Unknown codec time conversion accuracy");
    return true;
}

/**
 * @brief Test invalid granule position handling
 */
bool test_invalid_granule_positions() {
    TimeConversionTest test;
    uint32_t vorbis_stream_id = 1001;
    
    // Test invalid granule positions (-1 and very large values)
    uint64_t invalid_granules[] = {
        static_cast<uint64_t>(-1),           // -1 (invalid marker)
        0x8000000000000000ULL,               // Very large value
        0xFFFFFFFFFFFFFFFFULL,               // Maximum uint64_t
        0x7FFFFFFFFFFFFFFFULL + 1,           // Just over the valid range
    };
    
    for (uint64_t invalid_granule : invalid_granules) {
        uint64_t result = test.demuxer->granuleToMs(invalid_granule, vorbis_stream_id);
        if (result != 0) {
            Debug::log("test", "FAIL: Invalid granule position should return 0 - granule=", invalid_granule, ", got=", result);
            return false;
        }
    }
    
    Debug::log("test", "PASS: Invalid granule position handling");
    return true;
}

/**
 * @brief Test invalid stream ID handling
 */
bool test_invalid_stream_ids() {
    TimeConversionTest test;
    
    // Test non-existent stream IDs
    uint32_t invalid_stream_ids[] = {0, 999, 2000, 0xFFFFFFFF};
    
    for (uint32_t invalid_id : invalid_stream_ids) {
        uint64_t result_ms = test.demuxer->granuleToMs(44100, invalid_id);
        if (result_ms != 0) {
            Debug::log("test", "FAIL: Invalid stream ID should return 0 for granuleToMs - stream_id=", invalid_id, ", got=", result_ms);
            return false;
        }
        
        uint64_t result_granule = test.demuxer->msToGranule(1000, invalid_id);
        if (result_granule != 0) {
            Debug::log("test", "FAIL: Invalid stream ID should return 0 for msToGranule - stream_id=", invalid_id, ", got=", result_granule);
            return false;
        }
    }
    
    Debug::log("test", "PASS: Invalid stream ID handling");
    return true;
}

/**
 * @brief Test zero sample rate handling
 */
bool test_zero_sample_rate() {
    TimeConversionTest test;
    auto& streams = test.demuxer->getStreamsForTesting();
    
    // Create a stream with zero sample rate
    OggStream invalid_stream;
    invalid_stream.serial_number = 9999;
    invalid_stream.codec_name = "vorbis";
    invalid_stream.codec_type = "audio";
    invalid_stream.sample_rate = 0;  // Invalid sample rate
    invalid_stream.channels = 2;
    invalid_stream.headers_complete = true;
    streams[9999] = invalid_stream;
    
    // Test that zero sample rate returns 0
    uint64_t result_ms = test.demuxer->granuleToMs(44100, 9999);
    if (result_ms != 0) {
        Debug::log("test", "FAIL: Zero sample rate should return 0 for granuleToMs - got=", result_ms);
        return false;
    }
    
    uint64_t result_granule = test.demuxer->msToGranule(1000, 9999);
    if (result_granule != 0) {
        Debug::log("test", "FAIL: Zero sample rate should return 0 for msToGranule - got=", result_granule);
        return false;
    }
    
    Debug::log("test", "PASS: Zero sample rate handling");
    return true;
}

/**
 * @brief Test precision and rounding behavior
 */
bool test_precision_and_rounding() {
    TimeConversionTest test;
    uint32_t vorbis_stream_id = 1001;  // 44.1kHz
    
    // Test cases that involve rounding
    struct PrecisionTest {
        uint64_t granule;
        uint64_t expected_ms_min;
        uint64_t expected_ms_max;
        const char* description;
    };
    
    PrecisionTest precision_tests[] = {
        {441, 9, 10, "10ms with rounding (441 samples / 44100 = 9.99ms)"},
        {882, 19, 20, "20ms with rounding (882 samples / 44100 = 19.99ms)"},
        {22, 0, 1, "Sub-millisecond precision (22 samples / 44100 = 0.49ms)"},
        {23, 0, 1, "Sub-millisecond precision (23 samples / 44100 = 0.52ms)"},
    };
    
    for (const auto& tc : precision_tests) {
        uint64_t result_ms = test.demuxer->granuleToMs(tc.granule, vorbis_stream_id);
        if (result_ms < tc.expected_ms_min || result_ms > tc.expected_ms_max) {
            Debug::log("test", "FAIL: Precision test - ", tc.description, 
                       " - granule=", tc.granule, ", expected=[", tc.expected_ms_min, "-", tc.expected_ms_max, "]ms, got=", result_ms, "ms");
            return false;
        }
    }
    
    Debug::log("test", "PASS: Precision and rounding behavior");
    return true;
}

/**
 * @brief Test large values and edge cases
 */
bool test_large_values() {
    TimeConversionTest test;
    uint32_t vorbis_stream_id = 1001;  // 44.1kHz
    
    // Test large but valid values
    struct LargeValueTest {
        uint64_t granule;
        uint64_t expected_ms;
        const char* description;
    };
    
    LargeValueTest large_tests[] = {
        {44100000, 1000000, "1000 seconds (16.67 minutes)"},
        {441000000, 10000000, "10000 seconds (2.78 hours)"},
        {4410000000ULL, 100000000ULL, "100000 seconds (27.78 hours)"},
    };
    
    for (const auto& tc : large_tests) {
        uint64_t result_ms = test.demuxer->granuleToMs(tc.granule, vorbis_stream_id);
        if (result_ms != tc.expected_ms) {
            Debug::log("test", "FAIL: Large value test - ", tc.description, 
                       " - granule=", tc.granule, ", expected=", tc.expected_ms, "ms, got=", result_ms, "ms");
            return false;
        }
    }
    
    Debug::log("test", "PASS: Large values and edge cases");
    return true;
}

/**
 * @brief Test Opus pre-skip edge cases
 */
bool test_opus_preskip_edge_cases() {
    TimeConversionTest test;
    auto& streams = test.demuxer->getStreamsForTesting();
    
    // Create Opus stream with large pre-skip value
    OggStream opus_large_preskip;
    opus_large_preskip.serial_number = 2001;
    opus_large_preskip.codec_name = "opus";
    opus_large_preskip.codec_type = "audio";
    opus_large_preskip.sample_rate = 48000;
    opus_large_preskip.channels = 2;
    opus_large_preskip.pre_skip = 50000;  // Suspiciously large pre-skip
    opus_large_preskip.headers_complete = true;
    streams[2001] = opus_large_preskip;
    
    // Test that large pre-skip is handled (should still work but log warning)
    uint64_t result_ms = test.demuxer->granuleToMs(100000, 2001);
    uint64_t expected_ms = (100000 - 50000) * 1000 / 48000;  // Should be ~1041ms
    if (result_ms != expected_ms) {
        Debug::log("test", "FAIL: Large pre-skip handling - expected=", expected_ms, "ms, got=", result_ms, "ms");
        return false;
    }
    
    // Test granule less than pre-skip
    result_ms = test.demuxer->granuleToMs(25000, 2001);  // Less than pre-skip
    if (result_ms != 0) {
        Debug::log("test", "FAIL: Granule less than pre-skip should return 0 - got=", result_ms, "ms");
        return false;
    }
    
    Debug::log("test", "PASS: Opus pre-skip edge cases");
    return true;
}

int main() {
    printf("Starting OggDemuxer time conversion tests...\n");
    
    bool all_passed = true;
    int test_count = 0;
    int passed_count = 0;
    
    printf("Running test_vorbis_time_conversion...\n");
    test_count++;
    if (test_vorbis_time_conversion()) { 
        printf("PASS: test_vorbis_time_conversion\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_vorbis_time_conversion\n"); 
        all_passed = false; 
    }
    
    printf("Running test_opus_time_conversion...\n");
    test_count++;
    if (test_opus_time_conversion()) { 
        printf("PASS: test_opus_time_conversion\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_opus_time_conversion\n"); 
        all_passed = false; 
    }
    
    printf("Running test_flac_time_conversion...\n");
    test_count++;
    if (test_flac_time_conversion()) { 
        printf("PASS: test_flac_time_conversion\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_flac_time_conversion\n"); 
        all_passed = false; 
    }
    
    printf("Running test_speex_time_conversion...\n");
    test_count++;
    if (test_speex_time_conversion()) { 
        printf("PASS: test_speex_time_conversion\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_speex_time_conversion\n"); 
        all_passed = false; 
    }
    
    printf("Running test_unknown_codec_time_conversion...\n");
    test_count++;
    if (test_unknown_codec_time_conversion()) { 
        printf("PASS: test_unknown_codec_time_conversion\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_unknown_codec_time_conversion\n"); 
        all_passed = false; 
    }
    
    printf("Running test_invalid_granule_positions...\n");
    test_count++;
    if (test_invalid_granule_positions()) { 
        printf("PASS: test_invalid_granule_positions\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_invalid_granule_positions\n"); 
        all_passed = false; 
    }
    
    printf("Running test_invalid_stream_ids...\n");
    test_count++;
    if (test_invalid_stream_ids()) { 
        printf("PASS: test_invalid_stream_ids\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_invalid_stream_ids\n"); 
        all_passed = false; 
    }
    
    printf("Running test_zero_sample_rate...\n");
    test_count++;
    if (test_zero_sample_rate()) { 
        printf("PASS: test_zero_sample_rate\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_zero_sample_rate\n"); 
        all_passed = false; 
    }
    
    printf("Running test_precision_and_rounding...\n");
    test_count++;
    if (test_precision_and_rounding()) { 
        printf("PASS: test_precision_and_rounding\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_precision_and_rounding\n"); 
        all_passed = false; 
    }
    
    printf("Running test_large_values...\n");
    test_count++;
    if (test_large_values()) { 
        printf("PASS: test_large_values\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_large_values\n"); 
        all_passed = false; 
    }
    
    printf("Running test_opus_preskip_edge_cases...\n");
    test_count++;
    if (test_opus_preskip_edge_cases()) { 
        printf("PASS: test_opus_preskip_edge_cases\n"); 
        passed_count++;
    } else { 
        printf("FAIL: test_opus_preskip_edge_cases\n"); 
        all_passed = false; 
    }
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", test_count);
    printf("Tests passed: %d\n", passed_count);
    printf("Tests failed: %d\n", test_count - passed_count);
    
    if (all_passed) {
        printf("All time conversion tests PASSED!\n");
        return 0;
    } else {
        printf("Some time conversion tests FAILED!\n");
        return 1;
    }
}

#else
int main() {
    printf("OggDemuxer not available - skipping time conversion tests\n");
    return 0;
}
#endif // HAVE_OGGDEMUXER