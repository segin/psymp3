/*
 * test_ogg_granule_conversion.cpp - Unit tests for OggDemuxer granule position conversion
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include <iostream>
#include <cassert>
#include <memory>
#include <vector>

// Mock IOHandler for testing
class MockIOHandler : public IOHandler {
public:
    MockIOHandler() : m_data(), m_position(0) {}
    
    size_t read(void* buffer, size_t size, size_t count) override {
        size_t bytes_to_read = size * count;
        size_t available = m_data.size() - m_position;
        size_t actual_read = std::min(bytes_to_read, available);
        
        if (actual_read > 0) {
            std::memcpy(buffer, m_data.data() + m_position, actual_read);
            m_position += actual_read;
        }
        
        return actual_read / size;
    }
    

    
    int seek(long offset, int whence) override {
        switch (whence) {
            case SEEK_SET:
                m_position = offset;
                break;
            case SEEK_CUR:
                m_position += offset;
                break;
            case SEEK_END:
                m_position = m_data.size() + offset;
                break;
        }
        
        if (m_position < 0) m_position = 0;
        if (m_position > m_data.size()) m_position = m_data.size();
        
        return 0;
    }
    
    long tell() override {
        return m_position;
    }
    
    bool eof() override {
        return m_position >= m_data.size();
    }
    
    off_t getFileSize() override {
        return m_data.size();
    }
    
    void setData(const std::vector<uint8_t>& data) {
        m_data = data;
        m_position = 0;
    }
    
private:
    std::vector<uint8_t> m_data;
    size_t m_position;
};

// Test helper class to access methods and add test streams
class TestableOggDemuxer : public OggDemuxer {
public:
    TestableOggDemuxer(std::unique_ptr<IOHandler> handler) : OggDemuxer(std::move(handler)) {}
    
    // Helper to create test streams
    void addTestStream(uint32_t stream_id, const std::string& codec_name, 
                      uint32_t sample_rate, uint16_t channels, uint64_t pre_skip = 0) {
        OggStream stream;
        stream.serial_number = stream_id;
        stream.codec_name = codec_name;
        stream.codec_type = "audio";
        stream.sample_rate = sample_rate;
        stream.channels = channels;
        stream.pre_skip = pre_skip;
        stream.headers_complete = true;
        
        getStreamsForTesting()[stream_id] = stream;
    }
};

void test_opus_granule_conversion() {
    std::cout << "Testing Opus granule position conversion..." << std::endl;
    
    auto mock_handler = std::make_unique<MockIOHandler>();
    TestableOggDemuxer demuxer(std::move(mock_handler));
    
    // Create test Opus stream with typical pre-skip value
    uint32_t stream_id = 1;
    uint32_t sample_rate = 48000; // Opus output rate
    uint64_t pre_skip = 312; // Typical Opus pre-skip value
    
    demuxer.addTestStream(stream_id, "opus", sample_rate, 2, pre_skip);
    
    // Test cases for Opus granule conversion
    struct TestCase {
        uint64_t granule;
        uint64_t expected_ms;
        const char* description;
    };
    
    std::vector<TestCase> test_cases = {
        {0, 0, "granule 0 -> 0ms"},
        {pre_skip, 0, "granule at pre-skip -> 0ms"},
        {pre_skip + 48000, 1000, "granule pre-skip + 48000 -> 1000ms (1 second)"},
        {pre_skip + 24000, 500, "granule pre-skip + 24000 -> 500ms (0.5 seconds)"},
        {pre_skip + 144000, 3000, "granule pre-skip + 144000 -> 3000ms (3 seconds)"},
        {100, 0, "granule < pre-skip -> 0ms"},
    };
    
    for (const auto& test_case : test_cases) {
        uint64_t result = demuxer.granuleToMs(test_case.granule, stream_id);
        if (result != test_case.expected_ms) {
            std::cerr << "FAIL: " << test_case.description 
                      << " - Expected: " << test_case.expected_ms 
                      << "ms, Got: " << result << "ms" << std::endl;
            assert(false);
        }
        std::cout << "✓ " << test_case.description << std::endl;
    }
    
    // Test reverse conversion (ms to granule)
    struct ReverseTestCase {
        uint64_t timestamp_ms;
        uint64_t expected_granule;
        const char* description;
    };
    
    std::vector<ReverseTestCase> reverse_cases = {
        {0, pre_skip, "0ms -> pre-skip granule"},
        {1000, pre_skip + 48000, "1000ms -> pre-skip + 48000"},
        {500, pre_skip + 24000, "500ms -> pre-skip + 24000"},
        {3000, pre_skip + 144000, "3000ms -> pre-skip + 144000"},
    };
    
    for (const auto& test_case : reverse_cases) {
        uint64_t result = demuxer.msToGranule(test_case.timestamp_ms, stream_id);
        if (result != test_case.expected_granule) {
            std::cerr << "FAIL: " << test_case.description 
                      << " - Expected: " << test_case.expected_granule 
                      << ", Got: " << result << std::endl;
            assert(false);
        }
        std::cout << "✓ " << test_case.description << std::endl;
    }
    
    std::cout << "✓ Opus granule conversion tests passed" << std::endl;
}

void test_vorbis_granule_conversion() {
    std::cout << "Testing Vorbis granule position conversion..." << std::endl;
    
    auto mock_handler = std::make_unique<MockIOHandler>();
    TestableOggDemuxer demuxer(std::move(mock_handler));
    
    // Create test Vorbis stream
    uint32_t stream_id = 2;
    uint32_t sample_rate = 44100; // Common Vorbis sample rate
    
    demuxer.addTestStream(stream_id, "vorbis", sample_rate, 2);
    
    // Test cases for Vorbis granule conversion
    struct TestCase {
        uint64_t granule;
        uint64_t expected_ms;
        const char* description;
    };
    
    std::vector<TestCase> test_cases = {
        {0, 0, "granule 0 -> 0ms"},
        {44100, 1000, "granule 44100 -> 1000ms (1 second at 44.1kHz)"},
        {22050, 500, "granule 22050 -> 500ms (0.5 seconds at 44.1kHz)"},
        {132300, 3000, "granule 132300 -> 3000ms (3 seconds at 44.1kHz)"},
        {88200, 2000, "granule 88200 -> 2000ms (2 seconds at 44.1kHz)"},
    };
    
    for (const auto& test_case : test_cases) {
        uint64_t result = demuxer.granuleToMs(test_case.granule, stream_id);
        if (result != test_case.expected_ms) {
            std::cerr << "FAIL: " << test_case.description 
                      << " - Expected: " << test_case.expected_ms 
                      << "ms, Got: " << result << "ms" << std::endl;
            assert(false);
        }
        std::cout << "✓ " << test_case.description << std::endl;
    }
    
    // Test reverse conversion (ms to granule)
    struct ReverseTestCase {
        uint64_t timestamp_ms;
        uint64_t expected_granule;
        const char* description;
    };
    
    std::vector<ReverseTestCase> reverse_cases = {
        {0, 0, "0ms -> 0 granule"},
        {1000, 44100, "1000ms -> 44100 granule"},
        {500, 22050, "500ms -> 22050 granule"},
        {3000, 132300, "3000ms -> 132300 granule"},
        {2000, 88200, "2000ms -> 88200 granule"},
    };
    
    for (const auto& test_case : reverse_cases) {
        uint64_t result = demuxer.msToGranule(test_case.timestamp_ms, stream_id);
        if (result != test_case.expected_granule) {
            std::cerr << "FAIL: " << test_case.description 
                      << " - Expected: " << test_case.expected_granule 
                      << ", Got: " << result << std::endl;
            assert(false);
        }
        std::cout << "✓ " << test_case.description << std::endl;
    }
    
    std::cout << "✓ Vorbis granule conversion tests passed" << std::endl;
}

void test_flac_granule_conversion() {
    std::cout << "Testing FLAC-in-Ogg granule position conversion..." << std::endl;
    
    auto mock_handler = std::make_unique<MockIOHandler>();
    TestableOggDemuxer demuxer(std::move(mock_handler));
    
    // Create test FLAC stream
    uint32_t stream_id = 3;
    uint32_t sample_rate = 44100; // Common FLAC sample rate
    
    demuxer.addTestStream(stream_id, "flac", sample_rate, 2);
    
    // Test cases for FLAC granule conversion (same as Vorbis - sample-based)
    struct TestCase {
        uint64_t granule;
        uint64_t expected_ms;
        const char* description;
    };
    
    std::vector<TestCase> test_cases = {
        {0, 0, "granule 0 -> 0ms"},
        {44100, 1000, "granule 44100 -> 1000ms (1 second at 44.1kHz)"},
        {22050, 500, "granule 22050 -> 500ms (0.5 seconds at 44.1kHz)"},
        {132300, 3000, "granule 132300 -> 3000ms (3 seconds at 44.1kHz)"},
        {88200, 2000, "granule 88200 -> 2000ms (2 seconds at 44.1kHz)"},
    };
    
    for (const auto& test_case : test_cases) {
        uint64_t result = demuxer.granuleToMs(test_case.granule, stream_id);
        if (result != test_case.expected_ms) {
            std::cerr << "FAIL: " << test_case.description 
                      << " - Expected: " << test_case.expected_ms 
                      << "ms, Got: " << result << "ms" << std::endl;
            assert(false);
        }
        std::cout << "✓ " << test_case.description << std::endl;
    }
    
    // Test reverse conversion (ms to granule)
    struct ReverseTestCase {
        uint64_t timestamp_ms;
        uint64_t expected_granule;
        const char* description;
    };
    
    std::vector<ReverseTestCase> reverse_cases = {
        {0, 0, "0ms -> 0 granule"},
        {1000, 44100, "1000ms -> 44100 granule"},
        {500, 22050, "500ms -> 22050 granule"},
        {3000, 132300, "3000ms -> 132300 granule"},
        {2000, 88200, "2000ms -> 88200 granule"},
    };
    
    for (const auto& test_case : reverse_cases) {
        uint64_t result = demuxer.msToGranule(test_case.timestamp_ms, stream_id);
        if (result != test_case.expected_granule) {
            std::cerr << "FAIL: " << test_case.description 
                      << " - Expected: " << test_case.expected_granule 
                      << ", Got: " << result << std::endl;
            assert(false);
        }
        std::cout << "✓ " << test_case.description << std::endl;
    }
    
    std::cout << "✓ FLAC-in-Ogg granule conversion tests passed" << std::endl;
}

void test_different_sample_rates() {
    std::cout << "Testing granule conversion with different sample rates..." << std::endl;
    
    auto mock_handler = std::make_unique<MockIOHandler>();
    TestableOggDemuxer demuxer(std::move(mock_handler));
    
    // Test different sample rates for Vorbis
    struct SampleRateTest {
        uint32_t sample_rate;
        uint64_t granule_for_1_second;
        const char* description;
    };
    
    std::vector<SampleRateTest> sample_rate_tests = {
        {8000, 8000, "8kHz sample rate"},
        {16000, 16000, "16kHz sample rate"},
        {22050, 22050, "22.05kHz sample rate"},
        {44100, 44100, "44.1kHz sample rate"},
        {48000, 48000, "48kHz sample rate"},
        {96000, 96000, "96kHz sample rate"},
    };
    
    for (size_t i = 0; i < sample_rate_tests.size(); ++i) {
        const auto& test = sample_rate_tests[i];
        uint32_t stream_id = 100 + i;
        
        demuxer.addTestStream(stream_id, "vorbis", test.sample_rate, 2);
        
        // Test 1 second conversion
        uint64_t result_ms = demuxer.granuleToMs(test.granule_for_1_second, stream_id);
        if (result_ms != 1000) {
            std::cerr << "FAIL: " << test.description 
                      << " - Expected: 1000ms, Got: " << result_ms << "ms" << std::endl;
            assert(false);
        }
        
        // Test reverse conversion
        uint64_t result_granule = demuxer.msToGranule(1000, stream_id);
        if (result_granule != test.granule_for_1_second) {
            std::cerr << "FAIL: " << test.description << " (reverse)"
                      << " - Expected: " << test.granule_for_1_second 
                      << ", Got: " << result_granule << std::endl;
            assert(false);
        }
        
        std::cout << "✓ " << test.description << std::endl;
    }
    
    std::cout << "✓ Different sample rate tests passed" << std::endl;
}

void test_invalid_inputs() {
    std::cout << "Testing invalid input handling..." << std::endl;
    
    auto mock_handler = std::make_unique<MockIOHandler>();
    TestableOggDemuxer demuxer(std::move(mock_handler));
    
    // Create a valid test stream
    uint32_t valid_stream_id = 1;
    demuxer.addTestStream(valid_stream_id, "vorbis", 44100, 2);
    
    // Test invalid stream ID
    uint64_t result = demuxer.granuleToMs(44100, 999); // Non-existent stream
    assert(result == 0);
    std::cout << "✓ Invalid stream ID returns 0" << std::endl;
    
    // Test invalid granule position
    result = demuxer.granuleToMs(static_cast<uint64_t>(-1), valid_stream_id);
    assert(result == 0);
    std::cout << "✓ Invalid granule position (-1) returns 0" << std::endl;
    
    // Test very large granule position
    result = demuxer.granuleToMs(0x8000000000000000ULL, valid_stream_id);
    assert(result == 0);
    std::cout << "✓ Very large granule position returns 0" << std::endl;
    
    // Test stream with zero sample rate
    uint32_t invalid_stream_id = 2;
    demuxer.addTestStream(invalid_stream_id, "vorbis", 0, 2); // Invalid sample rate
    
    result = demuxer.granuleToMs(44100, invalid_stream_id);
    assert(result == 0);
    std::cout << "✓ Stream with zero sample rate returns 0" << std::endl;
    
    // Test reverse conversion with invalid inputs
    result = demuxer.msToGranule(1000, 999); // Non-existent stream
    assert(result == 0);
    std::cout << "✓ Invalid stream ID in msToGranule returns 0" << std::endl;
    
    result = demuxer.msToGranule(1000, invalid_stream_id); // Zero sample rate
    assert(result == 0);
    std::cout << "✓ Zero sample rate in msToGranule returns 0" << std::endl;
    
    std::cout << "✓ Invalid input handling tests passed" << std::endl;
}

void test_opus_edge_cases() {
    std::cout << "Testing Opus edge cases..." << std::endl;
    
    auto mock_handler = std::make_unique<MockIOHandler>();
    TestableOggDemuxer demuxer(std::move(mock_handler));
    
    // Test with different pre-skip values
    struct PreSkipTest {
        uint64_t pre_skip;
        const char* description;
    };
    
    std::vector<PreSkipTest> pre_skip_tests = {
        {0, "zero pre-skip"},
        {312, "typical pre-skip (312)"},
        {3840, "large pre-skip (3840)"},
        {1, "minimal pre-skip (1)"},
    };
    
    for (size_t i = 0; i < pre_skip_tests.size(); ++i) {
        const auto& test = pre_skip_tests[i];
        uint32_t stream_id = 200 + i;
        
        demuxer.addTestStream(stream_id, "opus", 48000, 2, test.pre_skip);
        
        // Test that granule equal to pre-skip gives 0ms
        uint64_t result = demuxer.granuleToMs(test.pre_skip, stream_id);
        if (result != 0) {
            std::cerr << "FAIL: " << test.description 
                      << " - granule=pre_skip should give 0ms, got: " << result << "ms" << std::endl;
            assert(false);
        }
        
        // Test that 0ms gives pre-skip granule
        uint64_t granule_result = demuxer.msToGranule(0, stream_id);
        if (granule_result != test.pre_skip) {
            std::cerr << "FAIL: " << test.description 
                      << " - 0ms should give pre_skip granule, got: " << granule_result << std::endl;
            assert(false);
        }
        
        std::cout << "✓ " << test.description << std::endl;
    }
    
    std::cout << "✓ Opus edge case tests passed" << std::endl;
}

int main() {
    std::cout << "Running OggDemuxer granule position conversion tests..." << std::endl;
    
    try {
        test_opus_granule_conversion();
        test_vorbis_granule_conversion();
        test_flac_granule_conversion();
        test_different_sample_rates();
        test_invalid_inputs();
        test_opus_edge_cases();
        
        std::cout << std::endl << "✓ All OggDemuxer granule conversion tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}

#else
int main() {
    std::cout << "OggDemuxer not available (HAVE_OGGDEMUXER not defined)" << std::endl;
    return 0;
}
#endif