/*
 * test_position_reporting_property.cpp - Property test for position reporting consistency
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * **Feature: ogg-demuxer-fix, Property 17: Position Reporting Consistency**
 * **Validates: Requirements 14.4**
 *
 * Property: For any position query, the demuxer SHALL return timestamps in
 * milliseconds, calculated consistently from granule positions using
 * codec-specific sample rates.
 */

#include "psymp3.h"

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>
#endif

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <memory>
#include <cmath>

#ifdef HAVE_OGGDEMUXER
using namespace PsyMP3::Demuxer::Ogg;
#endif

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            std::cout << "✗ FAILED: " << msg << std::endl; \
            tests_failed++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(msg) \
    do { \
        std::cout << "✓ " << msg << std::endl; \
        tests_passed++; \
    } while(0)

#ifdef HAVE_OGGDEMUXER

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
 * @brief Test fixture for position reporting tests
 */
class PositionReportingTest {
public:
    PositionReportingTest() {
        auto mock_handler = std::make_unique<MockIOHandler>();
        demuxer = std::make_unique<OggDemuxer>(std::move(mock_handler));
        setupTestStreams();
    }
    
    void setupTestStreams() {
        auto& streams = demuxer->getStreamsForTesting();
        
        // Vorbis stream (44.1kHz)
        OggStream vorbis_stream;
        vorbis_stream.serial_number = 1001;
        vorbis_stream.codec_name = "vorbis";
        vorbis_stream.codec_type = "audio";
        vorbis_stream.sample_rate = 44100;
        vorbis_stream.channels = 2;
        vorbis_stream.headers_complete = true;
        streams[1001] = vorbis_stream;
        
        // Opus stream (48kHz with pre-skip)
        OggStream opus_stream;
        opus_stream.serial_number = 1002;
        opus_stream.codec_name = "opus";
        opus_stream.codec_type = "audio";
        opus_stream.sample_rate = 48000;
        opus_stream.channels = 2;
        opus_stream.pre_skip = 312;
        opus_stream.headers_complete = true;
        streams[1002] = opus_stream;
        
        // FLAC stream (96kHz)
        OggStream flac_stream;
        flac_stream.serial_number = 1003;
        flac_stream.codec_name = "flac";
        flac_stream.codec_type = "audio";
        flac_stream.sample_rate = 96000;
        flac_stream.channels = 2;
        flac_stream.headers_complete = true;
        streams[1003] = flac_stream;
    }
    
    std::unique_ptr<OggDemuxer> demuxer;
};

// ============================================================================
// **Feature: ogg-demuxer-fix, Property 17: Position Reporting Consistency**
// **Validates: Requirements 14.4**
// ============================================================================

/**
 * @brief Property: Position is always reported in milliseconds
 * 
 * For any granule position and codec type, the position returned by
 * granuleToMs() must be in milliseconds (not samples, not seconds).
 */
bool test_property17_position_in_milliseconds() {
    PositionReportingTest test;
    
    // Test that 1 second of samples converts to 1000ms for each codec
    struct TestCase {
        uint32_t stream_id;
        uint64_t one_second_granule;
        const char* codec_name;
    };
    
    TestCase test_cases[] = {
        {1001, 44100, "vorbis"},   // 44100 samples = 1 second
        {1002, 48312, "opus"},     // 48000 + 312 pre-skip = 1 second after pre-skip
        {1003, 96000, "flac"},     // 96000 samples = 1 second
    };
    
    for (const auto& tc : test_cases) {
        uint64_t result_ms = test.demuxer->granuleToMs(tc.one_second_granule, tc.stream_id);
        
        // Allow small rounding error (±1ms)
        if (result_ms < 999 || result_ms > 1001) {
            std::cout << "✗ FAILED: " << tc.codec_name << " 1 second granule should be ~1000ms, got " 
                      << result_ms << "ms" << std::endl;
            tests_failed++;
            return false;
        }
    }
    
    TEST_PASS("Property 17: Position is always reported in milliseconds");
    return true;
}

/**
 * @brief Property: Position calculation is consistent across codecs
 * 
 * For any codec, the formula granule_to_ms(ms_to_granule(X)) should return
 * approximately X (within rounding tolerance).
 */
bool test_property17_round_trip_consistency() {
    PositionReportingTest test;
    
    uint32_t stream_ids[] = {1001, 1002, 1003};  // vorbis, opus, flac
    uint64_t test_timestamps[] = {0, 100, 500, 1000, 5000, 10000, 60000, 300000};
    
    for (uint32_t stream_id : stream_ids) {
        for (uint64_t timestamp_ms : test_timestamps) {
            // Convert ms -> granule -> ms
            uint64_t granule = test.demuxer->msToGranule(timestamp_ms, stream_id);
            uint64_t result_ms = test.demuxer->granuleToMs(granule, stream_id);
            
            // Allow small rounding error (±1ms)
            int64_t diff = static_cast<int64_t>(result_ms) - static_cast<int64_t>(timestamp_ms);
            if (diff < -1 || diff > 1) {
                std::cout << "✗ FAILED: Round-trip for stream " << stream_id 
                          << " timestamp " << timestamp_ms << "ms: got " << result_ms << "ms" << std::endl;
                tests_failed++;
                return false;
            }
        }
    }
    
    TEST_PASS("Property 17: Round-trip position conversion is consistent");
    return true;
}

/**
 * @brief Property: Position is monotonically increasing with granule
 * 
 * For any codec, if granule_a < granule_b, then granuleToMs(granule_a) <= granuleToMs(granule_b).
 */
bool test_property17_monotonic_position() {
    PositionReportingTest test;
    
    uint32_t stream_ids[] = {1001, 1002, 1003};
    
    for (uint32_t stream_id : stream_ids) {
        uint64_t prev_ms = 0;
        
        // Test with increasing granule positions
        for (uint64_t granule = 0; granule < 1000000; granule += 10000) {
            uint64_t current_ms = test.demuxer->granuleToMs(granule, stream_id);
            
            if (current_ms < prev_ms) {
                std::cout << "✗ FAILED: Position not monotonic for stream " << stream_id 
                          << " at granule " << granule << std::endl;
                tests_failed++;
                return false;
            }
            
            prev_ms = current_ms;
        }
    }
    
    TEST_PASS("Property 17: Position is monotonically increasing with granule");
    return true;
}

/**
 * @brief Property: Invalid granule positions return 0
 * 
 * For any codec, granuleToMs(-1) and granuleToMs(FLAC_OGG_GRANULE_NO_PACKET)
 * should return 0 (unknown position).
 */
bool test_property17_invalid_granule_handling() {
    PositionReportingTest test;
    
    uint32_t stream_ids[] = {1001, 1002, 1003};
    
    for (uint32_t stream_id : stream_ids) {
        // Test -1 (invalid granule position per RFC 3533)
        uint64_t result1 = test.demuxer->granuleToMs(static_cast<uint64_t>(-1), stream_id);
        if (result1 != 0) {
            std::cout << "✗ FAILED: granuleToMs(-1) should return 0 for stream " << stream_id 
                      << ", got " << result1 << std::endl;
            tests_failed++;
            return false;
        }
        
        // Test FLAC_OGG_GRANULE_NO_PACKET (0xFFFFFFFFFFFFFFFF)
        uint64_t result2 = test.demuxer->granuleToMs(OggDemuxer::FLAC_OGG_GRANULE_NO_PACKET, stream_id);
        if (result2 != 0) {
            std::cout << "✗ FAILED: granuleToMs(FLAC_OGG_GRANULE_NO_PACKET) should return 0 for stream " 
                      << stream_id << ", got " << result2 << std::endl;
            tests_failed++;
            return false;
        }
    }
    
    TEST_PASS("Property 17: Invalid granule positions return 0");
    return true;
}

/**
 * @brief Property: Opus pre-skip is correctly accounted for
 * 
 * For Opus streams, granuleToMs should subtract pre-skip before calculating time.
 */
bool test_property17_opus_preskip_handling() {
    PositionReportingTest test;
    
    uint32_t opus_stream_id = 1002;  // pre-skip = 312
    
    // Granule position equal to pre-skip should be 0ms
    uint64_t result1 = test.demuxer->granuleToMs(312, opus_stream_id);
    if (result1 != 0) {
        std::cout << "✗ FAILED: Opus granule=312 (pre-skip) should be 0ms, got " << result1 << std::endl;
        tests_failed++;
        return false;
    }
    
    // Granule position less than pre-skip should also be 0ms
    uint64_t result2 = test.demuxer->granuleToMs(100, opus_stream_id);
    if (result2 != 0) {
        std::cout << "✗ FAILED: Opus granule=100 (< pre-skip) should be 0ms, got " << result2 << std::endl;
        tests_failed++;
        return false;
    }
    
    // Granule position 48000 + 312 should be 1000ms (1 second after pre-skip)
    uint64_t result3 = test.demuxer->granuleToMs(48312, opus_stream_id);
    if (result3 != 1000) {
        std::cout << "✗ FAILED: Opus granule=48312 should be 1000ms, got " << result3 << std::endl;
        tests_failed++;
        return false;
    }
    
    TEST_PASS("Property 17: Opus pre-skip is correctly accounted for");
    return true;
}

/**
 * @brief Property: Non-existent stream returns 0
 * 
 * For any non-existent stream ID, granuleToMs should return 0.
 */
bool test_property17_nonexistent_stream() {
    PositionReportingTest test;
    
    uint32_t nonexistent_stream_id = 9999;
    
    uint64_t result = test.demuxer->granuleToMs(44100, nonexistent_stream_id);
    if (result != 0) {
        std::cout << "✗ FAILED: granuleToMs for non-existent stream should return 0, got " 
                  << result << std::endl;
        tests_failed++;
        return false;
    }
    
    TEST_PASS("Property 17: Non-existent stream returns 0");
    return true;
}

#ifdef HAVE_RAPIDCHECK
/**
 * @brief RapidCheck property test for position reporting consistency
 */
void test_property17_rapidcheck() {
    std::cout << "Running RapidCheck property tests for Property 17..." << std::endl;
    
    // Property: Round-trip conversion is consistent for any timestamp
    rc::check("granuleToMs(msToGranule(x)) ≈ x for Vorbis", []() {
        PositionReportingTest test;
        uint64_t timestamp_ms = *rc::gen::inRange<uint64_t>(0, 3600000);  // 0 to 1 hour
        
        uint64_t granule = test.demuxer->msToGranule(timestamp_ms, 1001);
        uint64_t result_ms = test.demuxer->granuleToMs(granule, 1001);
        
        int64_t diff = static_cast<int64_t>(result_ms) - static_cast<int64_t>(timestamp_ms);
        RC_ASSERT(diff >= -1 && diff <= 1);
    });
    
    // Property: Position is monotonically increasing
    rc::check("granuleToMs is monotonic", []() {
        PositionReportingTest test;
        uint64_t granule_a = *rc::gen::inRange<uint64_t>(0, 1000000);
        uint64_t granule_b = *rc::gen::inRange<uint64_t>(granule_a, 2000000);
        
        uint64_t ms_a = test.demuxer->granuleToMs(granule_a, 1001);
        uint64_t ms_b = test.demuxer->granuleToMs(granule_b, 1001);
        
        RC_ASSERT(ms_a <= ms_b);
    });
    
    std::cout << "RapidCheck property tests completed." << std::endl;
}
#endif // HAVE_RAPIDCHECK

#endif // HAVE_OGGDEMUXER

int main() {
    std::cout << "=== Property 17: Position Reporting Consistency ===" << std::endl;
    std::cout << "**Feature: ogg-demuxer-fix, Property 17: Position Reporting Consistency**" << std::endl;
    std::cout << "**Validates: Requirements 14.4**" << std::endl;
    std::cout << std::endl;
    
#ifdef HAVE_OGGDEMUXER
    // Run unit-style property tests
    test_property17_position_in_milliseconds();
    test_property17_round_trip_consistency();
    test_property17_monotonic_position();
    test_property17_invalid_granule_handling();
    test_property17_opus_preskip_handling();
    test_property17_nonexistent_stream();
    
#ifdef HAVE_RAPIDCHECK
    test_property17_rapidcheck();
#endif
    
    std::cout << std::endl;
    std::cout << "=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;
    
    return tests_failed > 0 ? 1 : 0;
#else
    std::cout << "OggDemuxer not available - skipping tests" << std::endl;
    return 0;
#endif
}

