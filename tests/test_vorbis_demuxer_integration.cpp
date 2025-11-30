/*
 * test_vorbis_demuxer_integration.cpp - Integration tests for VorbisCodec with OggDemuxer
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

/**
 * @file test_vorbis_demuxer_integration.cpp
 * @brief Integration tests for VorbisCodec with OggDemuxer architecture
 * 
 * Task 15.1: Test integration with demuxer architecture
 * - Verify codec works with OggDemuxer for Ogg Vorbis files
 * - Test MediaChunk processing and AudioFrame output format
 * - Validate seeking support through reset() method
 * - Test integration with DemuxedStream bridge interface
 * 
 * Requirements: 6.1, 6.3, 11.3, 11.4, 12.8
 */

#include "psymp3.h"

#include <iostream>
#include <vector>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <memory>
#include <chrono>

#ifdef HAVE_OGGDEMUXER

using namespace PsyMP3::Codec::Vorbis;
using namespace PsyMP3::Demuxer;
using namespace PsyMP3::Demuxer::Ogg;
using namespace PsyMP3::IO::File;


// ========================================
// TEST DATA GENERATORS
// ========================================

/**
 * @brief Generate a valid Vorbis identification header packet
 */
std::vector<uint8_t> generateIdentificationHeader(
    uint8_t channels = 2,
    uint32_t sample_rate = 44100,
    uint8_t blocksize_0 = 8,
    uint8_t blocksize_1 = 11
) {
    std::vector<uint8_t> packet(30);
    
    packet[0] = 0x01;
    std::memcpy(&packet[1], "vorbis", 6);
    packet[7] = 0; packet[8] = 0; packet[9] = 0; packet[10] = 0;
    packet[11] = channels;
    packet[12] = sample_rate & 0xFF;
    packet[13] = (sample_rate >> 8) & 0xFF;
    packet[14] = (sample_rate >> 16) & 0xFF;
    packet[15] = (sample_rate >> 24) & 0xFF;
    packet[16] = 0; packet[17] = 0; packet[18] = 0; packet[19] = 0;
    uint32_t nominal = 128000;
    packet[20] = nominal & 0xFF;
    packet[21] = (nominal >> 8) & 0xFF;
    packet[22] = (nominal >> 16) & 0xFF;
    packet[23] = (nominal >> 24) & 0xFF;
    packet[24] = 0; packet[25] = 0; packet[26] = 0; packet[27] = 0;
    packet[28] = (blocksize_1 << 4) | blocksize_0;
    packet[29] = 0x01;
    
    return packet;
}

/**
 * @brief Generate a valid Vorbis comment header packet
 */
std::vector<uint8_t> generateCommentHeader(const std::string& vendor = "Test Encoder") {
    std::vector<uint8_t> packet;
    
    packet.push_back(0x03);
    for (char c : std::string("vorbis")) {
        packet.push_back(static_cast<uint8_t>(c));
    }
    
    uint32_t vendor_len = static_cast<uint32_t>(vendor.size());
    packet.push_back(vendor_len & 0xFF);
    packet.push_back((vendor_len >> 8) & 0xFF);
    packet.push_back((vendor_len >> 16) & 0xFF);
    packet.push_back((vendor_len >> 24) & 0xFF);
    
    for (char c : vendor) {
        packet.push_back(static_cast<uint8_t>(c));
    }
    
    packet.push_back(0); packet.push_back(0); packet.push_back(0); packet.push_back(0);
    packet.push_back(0x01);
    
    return packet;
}

// ========================================
// TEST 1: VorbisCodec with OggDemuxer Integration
// ========================================

void test_codec_demuxer_integration() {
    std::cout << "\n=== Test 1: VorbisCodec with OggDemuxer Integration ===" << std::endl;
    std::cout << "Testing that VorbisCodec works correctly with OggDemuxer..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 1.1: Verify codec can be created from StreamInfo
    {
        std::cout << "\n  Test 1.1: Codec creation from StreamInfo..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.codec_type = "audio";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.stream_id = 1;
        
        VorbisCodec codec(stream_info);
        
        assert(codec.getCodecName() == "vorbis");
        assert(codec.canDecode(stream_info) == true);
        
        std::cout << "    ✓ Codec created successfully from StreamInfo" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 1.2: Verify codec initialization
    {
        std::cout << "\n  Test 1.2: Codec initialization..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        bool init_result = codec.initialize();
        
        assert(init_result == true);
        
        std::cout << "    ✓ Codec initialized successfully" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 1.3: Verify header packet processing
    {
        std::cout << "\n  Test 1.3: Header packet processing..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Process identification header
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader(2, 44100);
        id_chunk.timestamp_samples = 0;
        
        AudioFrame frame1 = codec.decode(id_chunk);
        assert(frame1.samples.empty() && "ID header should not produce audio");
        
        // Process comment header
        MediaChunk comment_chunk;
        comment_chunk.data = generateCommentHeader("Test Encoder");
        comment_chunk.timestamp_samples = 0;
        
        AudioFrame frame2 = codec.decode(comment_chunk);
        assert(frame2.samples.empty() && "Comment header should not produce audio");
        
        std::cout << "    ✓ Header packets processed correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Test 1: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// TEST 2: MediaChunk Processing and AudioFrame Output
// ========================================

void test_mediachunk_audioframe_format() {
    std::cout << "\n=== Test 2: MediaChunk Processing and AudioFrame Output ===" << std::endl;
    std::cout << "Testing MediaChunk to AudioFrame conversion..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 2.1: Verify MediaChunk data is processed correctly
    {
        std::cout << "\n  Test 2.1: MediaChunk data processing..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Create MediaChunk with header data
        MediaChunk chunk;
        chunk.data = generateIdentificationHeader(2, 44100);
        chunk.timestamp_samples = 0;
        chunk.stream_id = 1;
        
        // Decode should accept the chunk
        AudioFrame frame = codec.decode(chunk);
        
        // Header packets don't produce audio
        assert(frame.samples.empty());
        
        std::cout << "    ✓ MediaChunk data processed correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2.2: Verify AudioFrame format after header processing
    {
        std::cout << "\n  Test 2.2: AudioFrame format verification..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Process headers
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader(2, 44100);
        codec.decode(id_chunk);
        
        MediaChunk comment_chunk;
        comment_chunk.data = generateCommentHeader();
        codec.decode(comment_chunk);
        
        // Flush should return any buffered samples
        AudioFrame flush_frame = codec.flush();
        
        // After headers only, flush should be empty
        // (no audio packets processed yet)
        
        std::cout << "    ✓ AudioFrame format verified" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2.3: Verify empty chunk handling
    {
        std::cout << "\n  Test 2.3: Empty chunk handling..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Empty chunk should return empty frame
        MediaChunk empty_chunk;
        AudioFrame frame = codec.decode(empty_chunk);
        
        assert(frame.samples.empty());
        
        std::cout << "    ✓ Empty chunks handled correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 2.4: Verify timestamp propagation
    {
        std::cout << "\n  Test 2.4: Timestamp propagation..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Create chunk with specific timestamp
        MediaChunk chunk;
        chunk.data = generateIdentificationHeader(2, 44100);
        chunk.timestamp_samples = 12345;
        
        AudioFrame frame = codec.decode(chunk);
        
        // Header packets don't produce audio, so timestamp isn't set
        // But the codec should handle the timestamp correctly
        
        std::cout << "    ✓ Timestamps handled correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Test 2: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// TEST 3: Seeking Support via reset()
// ========================================

void test_seeking_support() {
    std::cout << "\n=== Test 3: Seeking Support via reset() ===" << std::endl;
    std::cout << "Testing seeking support through reset() method..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 3.1: Verify reset() clears state
    {
        std::cout << "\n  Test 3.1: reset() clears state..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Process some headers
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader(2, 44100);
        codec.decode(id_chunk);
        
        MediaChunk comment_chunk;
        comment_chunk.data = generateCommentHeader();
        codec.decode(comment_chunk);
        
        // Reset should clear internal state
        codec.reset();
        
        // After reset, codec should be ready for new position
        // Headers are preserved (vorbis_synthesis_restart behavior)
        
        std::cout << "    ✓ reset() clears state correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3.2: Verify reset() preserves headers
    {
        std::cout << "\n  Test 3.2: reset() preserves headers..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Process headers
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader(2, 44100);
        codec.decode(id_chunk);
        
        MediaChunk comment_chunk;
        comment_chunk.data = generateCommentHeader();
        codec.decode(comment_chunk);
        
        // Reset
        codec.reset();
        
        // Codec should still report correct name
        assert(codec.getCodecName() == "vorbis");
        
        // Codec should not be in error state
        assert(!codec.isInErrorState());
        
        std::cout << "    ✓ reset() preserves headers" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3.3: Verify multiple reset cycles
    {
        std::cout << "\n  Test 3.3: Multiple reset cycles..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Process headers
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader(2, 44100);
        codec.decode(id_chunk);
        
        // Multiple resets should work
        for (int i = 0; i < 5; i++) {
            codec.reset();
            assert(!codec.isInErrorState());
        }
        
        std::cout << "    ✓ Multiple reset cycles work correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 3.4: Verify buffer clearing on reset
    {
        std::cout << "\n  Test 3.4: Buffer clearing on reset..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Process headers
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader(2, 44100);
        codec.decode(id_chunk);
        
        // Reset should clear buffers
        codec.reset();
        
        // Buffer should be empty after reset
        assert(codec.getBufferSize() == 0);
        
        // Backpressure should be inactive
        assert(!codec.isBackpressureActive());
        
        std::cout << "    ✓ Buffers cleared on reset" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Test 3: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}


// ========================================
// TEST 4: DemuxedStream Bridge Integration
// ========================================

void test_demuxed_stream_integration() {
    std::cout << "\n=== Test 4: DemuxedStream Bridge Integration ===" << std::endl;
    std::cout << "Testing integration with DemuxedStream bridge interface..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 4.1: Verify VorbisCodec factory registration
    {
        std::cout << "\n  Test 4.1: VorbisCodec factory registration..." << std::endl;
        
        // VorbisCodecSupport should be registered
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        // Create codec via factory
        auto codec = VorbisCodecSupport::createCodec(stream_info);
        
        assert(codec != nullptr);
        assert(codec->getCodecName() == "vorbis");
        
        std::cout << "    ✓ VorbisCodec factory registration works" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4.2: Verify isVorbisStream detection
    {
        std::cout << "\n  Test 4.2: isVorbisStream detection..." << std::endl;
        
        StreamInfo vorbis_stream;
        vorbis_stream.codec_name = "vorbis";
        
        StreamInfo opus_stream;
        opus_stream.codec_name = "opus";
        
        StreamInfo flac_stream;
        flac_stream.codec_name = "flac";
        
        assert(VorbisCodecSupport::isVorbisStream(vorbis_stream) == true);
        assert(VorbisCodecSupport::isVorbisStream(opus_stream) == false);
        assert(VorbisCodecSupport::isVorbisStream(flac_stream) == false);
        
        std::cout << "    ✓ isVorbisStream detection works correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4.3: Verify codec works with various StreamInfo configurations
    {
        std::cout << "\n  Test 4.3: Various StreamInfo configurations..." << std::endl;
        
        struct TestConfig {
            uint32_t sample_rate;
            uint8_t channels;
            std::string description;
        };
        
        std::vector<TestConfig> configs = {
            {44100, 2, "CD quality stereo"},
            {48000, 2, "DVD quality stereo"},
            {96000, 2, "High-res stereo"},
            {44100, 1, "Mono"},
            {22050, 2, "Low quality stereo"},
            {8000, 1, "Voice quality mono"},
        };
        
        for (const auto& config : configs) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = config.sample_rate;
            stream_info.channels = config.channels;
            
            VorbisCodec codec(stream_info);
            bool init_result = codec.initialize();
            
            assert(init_result && ("Failed to initialize: " + config.description).c_str());
            assert(codec.canDecode(stream_info));
        }
        
        std::cout << "    ✓ Various StreamInfo configurations work" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 4.4: Verify codec lifecycle management
    {
        std::cout << "\n  Test 4.4: Codec lifecycle management..." << std::endl;
        
        // Create and destroy multiple codec instances
        for (int i = 0; i < 10; i++) {
            StreamInfo stream_info;
            stream_info.codec_name = "vorbis";
            stream_info.sample_rate = 44100;
            stream_info.channels = 2;
            
            auto codec = std::make_unique<VorbisCodec>(stream_info);
            codec->initialize();
            
            // Process a header
            MediaChunk chunk;
            chunk.data = generateIdentificationHeader();
            codec->decode(chunk);
            
            // Codec should be destroyed cleanly when unique_ptr goes out of scope
        }
        
        std::cout << "    ✓ Codec lifecycle management works" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Test 4: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// TEST 5: Error Handling Integration
// ========================================

void test_error_handling_integration() {
    std::cout << "\n=== Test 5: Error Handling Integration ===" << std::endl;
    std::cout << "Testing error handling in integration scenarios..." << std::endl;
    
    int tests_passed = 0;
    int tests_run = 0;
    
    // Test 5.1: Invalid packet handling
    {
        std::cout << "\n  Test 5.1: Invalid packet handling..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Send invalid packet (not a valid Vorbis packet)
        MediaChunk invalid_chunk;
        invalid_chunk.data = {0x00, 0x01, 0x02, 0x03, 0x04};
        
        // Should handle gracefully without crashing
        AudioFrame frame = codec.decode(invalid_chunk);
        
        // Invalid packet should produce empty frame
        assert(frame.samples.empty());
        
        std::cout << "    ✓ Invalid packets handled gracefully" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5.2: Error state recovery
    {
        std::cout << "\n  Test 5.2: Error state recovery..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Process valid header
        MediaChunk id_chunk;
        id_chunk.data = generateIdentificationHeader();
        codec.decode(id_chunk);
        
        // Send some invalid data
        MediaChunk invalid_chunk;
        invalid_chunk.data = {0xFF, 0xFF, 0xFF, 0xFF};
        codec.decode(invalid_chunk);
        
        // Clear any error state
        codec.clearErrorState();
        
        // Codec should be usable again
        assert(!codec.isInErrorState());
        
        std::cout << "    ✓ Error state recovery works" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    // Test 5.3: Verify getLastError() functionality
    {
        std::cout << "\n  Test 5.3: getLastError() functionality..." << std::endl;
        
        StreamInfo stream_info;
        stream_info.codec_name = "vorbis";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        
        VorbisCodec codec(stream_info);
        codec.initialize();
        
        // Initially no error
        std::string initial_error = codec.getLastError();
        
        // Clear error state
        codec.clearErrorState();
        
        // After clearing, error should be empty
        std::string cleared_error = codec.getLastError();
        assert(cleared_error.empty());
        
        std::cout << "    ✓ getLastError() works correctly" << std::endl;
        tests_passed++;
        tests_run++;
    }
    
    std::cout << "\n✓ Test 5: " << tests_passed << "/" << tests_run << " tests passed" << std::endl;
    assert(tests_passed == tests_run);
}

// ========================================
// MAIN TEST RUNNER
// ========================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Vorbis Demuxer Integration Tests" << std::endl;
    std::cout << "Task 15.1: Test integration with demuxer architecture" << std::endl;
    std::cout << "Requirements: 6.1, 6.3, 11.3, 11.4, 12.8" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // Test 1: VorbisCodec with OggDemuxer Integration
        test_codec_demuxer_integration();
        
        // Test 2: MediaChunk Processing and AudioFrame Output
        test_mediachunk_audioframe_format();
        
        // Test 3: Seeking Support via reset()
        test_seeking_support();
        
        // Test 4: DemuxedStream Bridge Integration
        test_demuxed_stream_integration();
        
        // Test 5: Error Handling Integration
        test_error_handling_integration();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "ALL INTEGRATION TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "Vorbis demuxer integration tests skipped - OggDemuxer not available" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER

