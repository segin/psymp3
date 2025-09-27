/*
 * test_opus_codec_error_handling.cpp - Test Opus codec error handling and edge cases
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#ifdef HAVE_OGGDEMUXER

using namespace TestFramework;

// ========== Test Data Creation Utilities ==========

/**
 * @brief Create a mock StreamInfo for Opus testing
 */
StreamInfo createOpusStreamInfo(uint16_t channels = 2, uint32_t sample_rate = 48000)
{
    StreamInfo info;
    info.codec_type = "audio";
    info.codec_name = "opus";
    info.channels = channels;
    info.sample_rate = sample_rate;
    info.bitrate = 128000;
    return info;
}

/**
 * @brief Create a valid OpusHead identification header packet
 */
std::vector<uint8_t> createValidOpusHeadPacket(uint8_t channels = 2)
{
    std::vector<uint8_t> packet;
    
    // OpusHead signature
    packet.insert(packet.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});
    
    // Version (1)
    packet.push_back(1);
    
    // Channel count
    packet.push_back(channels);
    
    // Pre-skip (312 samples, little endian)
    packet.push_back(0x38);
    packet.push_back(0x01);
    
    // Input sample rate (48000 Hz, little endian)
    packet.push_back(0x80);
    packet.push_back(0xBB);
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    // Output gain (0, little endian)
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    // Channel mapping family (0)
    packet.push_back(0x00);
    
    return packet;
}

/**
 * @brief Create a valid OpusTags comment header packet
 */
std::vector<uint8_t> createValidOpusTagsPacket()
{
    std::vector<uint8_t> packet;
    
    // OpusTags signature
    packet.insert(packet.end(), {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'});
    
    // Vendor string length (little endian)
    std::string vendor = "libopus 1.3.1";
    uint32_t vendor_len = vendor.length();
    packet.push_back(vendor_len & 0xFF);
    packet.push_back((vendor_len >> 8) & 0xFF);
    packet.push_back((vendor_len >> 16) & 0xFF);
    packet.push_back((vendor_len >> 24) & 0xFF);
    
    // Vendor string
    packet.insert(packet.end(), vendor.begin(), vendor.end());
    
    // User comment list length (0 comments)
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    
    return packet;
}

/**
 * @brief Create a corrupted OpusHead header packet
 */
std::vector<uint8_t> createCorruptedOpusHeadPacket()
{
    std::vector<uint8_t> packet;
    
    // Wrong signature
    packet.insert(packet.end(), {'B', 'a', 'd', 'H', 'e', 'a', 'd', '!'});
    
    // Invalid version
    packet.push_back(99);
    
    // Invalid channel count
    packet.push_back(0);
    
    // Truncated packet (missing required fields)
    return packet;
}

/**
 * @brief Create a corrupted audio packet
 */
std::vector<uint8_t> createCorruptedAudioPacket()
{
    std::vector<uint8_t> packet;
    
    // Invalid TOC byte
    packet.push_back(0xFF);
    
    // Random corrupted data
    packet.insert(packet.end(), {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE});
    
    return packet;
}

// ========== Test Cases ==========

/**
 * @brief Test handling of corrupted header packets
 */
class TestCorruptedHeaderHandling : public TestCase {
public:
    TestCorruptedHeaderHandling() : TestCase("Corrupted Header Packet Handling") {}
    
protected:
    void runTest() override {
        StreamInfo info = createOpusStreamInfo(2);
        OpusCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
        
        // Test corrupted identification header
        auto corrupted_head = createCorruptedOpusHeadPacket();
        MediaChunk corrupted_chunk;
        corrupted_chunk.data = corrupted_head;
        
        AudioFrame frame = codec.decode(corrupted_chunk);
        
        // Should handle corrupted header gracefully and return empty frame
        ASSERT_TRUE(frame.samples.empty(), "Corrupted header should return empty frame");
        
        // Test truncated header (too short)
        std::vector<uint8_t> truncated_header = {'O', 'p', 'u', 's'}; // Too short
        MediaChunk truncated_chunk;
        truncated_chunk.data = truncated_header;
        
        AudioFrame truncated_frame = codec.decode(truncated_chunk);
        ASSERT_TRUE(truncated_frame.samples.empty(), "Truncated header should return empty frame");
        
        // Test empty header packet
        MediaChunk empty_chunk;
        empty_chunk.data.clear();
        
        AudioFrame empty_frame = codec.decode(empty_chunk);
        ASSERT_TRUE(empty_frame.samples.empty(), "Empty packet should return empty frame");
        
        // Test header with wrong signature but correct length
        std::vector<uint8_t> wrong_sig = {'W', 'r', 'o', 'n', 'g', 'S', 'i', 'g'};
        wrong_sig.insert(wrong_sig.end(), 11, 0x00); // Pad to minimum length
        MediaChunk wrong_sig_chunk;
        wrong_sig_chunk.data = wrong_sig;
        
        AudioFrame wrong_sig_frame = codec.decode(wrong_sig_chunk);
        ASSERT_TRUE(wrong_sig_frame.samples.empty(), "Wrong signature header should return empty frame");
    }
};

/**
 * @brief Test handling of invalid header parameters
 */
class TestInvalidHeaderParameters : public TestCase {
public:
    TestInvalidHeaderParameters() : TestCase("Invalid Header Parameter Handling") {}
    
protected:
    void runTest() override {
        StreamInfo info = createOpusStreamInfo(2);
        
        // Test invalid version number
        {
            OpusCodec codec(info);
            ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
            
            std::vector<uint8_t> invalid_version_header;
            invalid_version_header.insert(invalid_version_header.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});
            invalid_version_header.push_back(0); // Invalid version (must be 1)
            invalid_version_header.insert(invalid_version_header.end(), 10, 0x00); // Pad
            
            MediaChunk chunk;
            chunk.data = invalid_version_header;
            
            AudioFrame frame = codec.decode(chunk);
            ASSERT_TRUE(frame.samples.empty(), "Invalid version header should return empty frame");
        }
        
        // Test invalid channel count (0 channels)
        {
            OpusCodec codec(info);
            ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
            
            std::vector<uint8_t> zero_channels_header;
            zero_channels_header.insert(zero_channels_header.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});
            zero_channels_header.push_back(1); // Valid version
            zero_channels_header.push_back(0); // Invalid channel count (0)
            zero_channels_header.insert(zero_channels_header.end(), 9, 0x00); // Pad
            
            MediaChunk chunk;
            chunk.data = zero_channels_header;
            
            AudioFrame frame = codec.decode(chunk);
            ASSERT_TRUE(frame.samples.empty(), "Zero channels header should return empty frame");
        }
        
        // Test excessive channel count (> 255)
        {
            OpusCodec codec(info);
            ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
            
            std::vector<uint8_t> excessive_channels_header;
            excessive_channels_header.insert(excessive_channels_header.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});
            excessive_channels_header.push_back(1); // Valid version
            excessive_channels_header.push_back(255); // Maximum channels (should be handled)
            excessive_channels_header.insert(excessive_channels_header.end(), 9, 0x00); // Pad
            
            MediaChunk chunk;
            chunk.data = excessive_channels_header;
            
            AudioFrame frame = codec.decode(chunk);
            // This might succeed or fail depending on implementation limits
            // Just verify it doesn't crash
        }
        
        // Test invalid mapping family
        {
            OpusCodec codec(info);
            ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
            
            std::vector<uint8_t> invalid_mapping_header = createValidOpusHeadPacket(2);
            invalid_mapping_header[invalid_mapping_header.size() - 1] = 99; // Invalid mapping family
            
            MediaChunk chunk;
            chunk.data = invalid_mapping_header;
            
            AudioFrame frame = codec.decode(chunk);
            ASSERT_TRUE(frame.samples.empty(), "Invalid mapping family header should return empty frame");
        }
    }
};

/**
 * @brief Test handling of corrupted audio packets
 */
class TestCorruptedAudioPackets : public TestCase {
public:
    TestCorruptedAudioPackets() : TestCase("Corrupted Audio Packet Handling") {}
    
protected:
    void runTest() override {
        StreamInfo info = createOpusStreamInfo(2);
        OpusCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
        
        // Process valid headers first
        auto head_packet = createValidOpusHeadPacket(2);
        auto tags_packet = createValidOpusTagsPacket();
        
        MediaChunk head_chunk;
        head_chunk.data = head_packet;
        MediaChunk tags_chunk;
        tags_chunk.data = tags_packet;
        
        AudioFrame head_frame = codec.decode(head_chunk);
        AudioFrame tags_frame = codec.decode(tags_chunk);
        
        ASSERT_TRUE(head_frame.samples.empty(), "Header frames should be empty");
        ASSERT_TRUE(tags_frame.samples.empty(), "Header frames should be empty");
        
        // Test corrupted audio packet
        auto corrupted_audio = createCorruptedAudioPacket();
        MediaChunk corrupted_chunk;
        corrupted_chunk.data = corrupted_audio;
        
        AudioFrame corrupted_frame = codec.decode(corrupted_chunk);
        
        // Should handle corrupted packet gracefully
        // Might return empty frame or silence frame depending on implementation
        if (!corrupted_frame.samples.empty()) {
            // If it returns samples, they should be valid (likely silence)
            ASSERT_EQUALS(2u, corrupted_frame.channels, "Corrupted packet recovery should have correct channels");
            ASSERT_EQUALS(48000u, corrupted_frame.sample_rate, "Corrupted packet recovery should have correct sample rate");
        }
        
        // Test extremely short audio packet
        std::vector<uint8_t> short_packet = {0x00}; // Just TOC byte
        MediaChunk short_chunk;
        short_chunk.data = short_packet;
        
        AudioFrame short_frame = codec.decode(short_chunk);
        // Should handle gracefully without crashing
        
        // Test packet with invalid TOC configuration
        std::vector<uint8_t> invalid_toc_packet = {0xFC, 0x00, 0x00}; // Invalid configuration
        MediaChunk invalid_toc_chunk;
        invalid_toc_chunk.data = invalid_toc_packet;
        
        AudioFrame invalid_toc_frame = codec.decode(invalid_toc_chunk);
        // Should handle gracefully without crashing
        
        // Test oversized packet (simulate memory stress)
        std::vector<uint8_t> oversized_packet(10000, 0xFF); // Large packet with invalid data
        MediaChunk oversized_chunk;
        oversized_chunk.data = oversized_packet;
        
        AudioFrame oversized_frame = codec.decode(oversized_chunk);
        // Should handle gracefully without excessive memory usage
    }
};

/**
 * @brief Test decoder state reset functionality
 */
class TestDecoderStateReset : public TestCase {
public:
    TestDecoderStateReset() : TestCase("Decoder State Reset Functionality") {}
    
protected:
    void runTest() override {
        StreamInfo info = createOpusStreamInfo(2);
        OpusCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
        
        // Process headers and some audio
        auto head_packet = createValidOpusHeadPacket(2);
        auto tags_packet = createValidOpusTagsPacket();
        
        MediaChunk head_chunk;
        head_chunk.data = head_packet;
        MediaChunk tags_chunk;
        tags_chunk.data = tags_packet;
        
        AudioFrame head_frame = codec.decode(head_chunk);
        AudioFrame tags_frame = codec.decode(tags_chunk);
        
        // Decode some audio to establish state
        std::vector<uint8_t> audio_packet = {0x00, 0x00}; // Minimal valid packet
        MediaChunk audio_chunk;
        audio_chunk.data = audio_packet;
        
        AudioFrame audio_frame1 = codec.decode(audio_chunk);
        
        // Reset codec state (for seeking support)
        codec.reset();
        
        // After reset, should be able to decode again
        AudioFrame audio_frame2 = codec.decode(audio_chunk);
        
        // Both frames should have similar characteristics if reset worked properly
        if (!audio_frame1.samples.empty() && !audio_frame2.samples.empty()) {
            ASSERT_EQUALS(audio_frame1.channels, audio_frame2.channels, 
                         "Channels should be consistent after reset");
            ASSERT_EQUALS(audio_frame1.sample_rate, audio_frame2.sample_rate, 
                         "Sample rate should be consistent after reset");
        }
        
        // Test multiple resets
        codec.reset();
        codec.reset();
        codec.reset();
        
        // Should still work after multiple resets
        AudioFrame audio_frame3 = codec.decode(audio_chunk);
        // Should not crash and should handle gracefully
        
        // Test reset after error state
        auto corrupted_packet = createCorruptedAudioPacket();
        MediaChunk corrupted_chunk;
        corrupted_chunk.data = corrupted_packet;
        
        // Cause error state
        AudioFrame error_frame = codec.decode(corrupted_chunk);
        
        // Reset should clear error state
        codec.reset();
        
        // Should be able to decode normally after reset
        AudioFrame recovery_frame = codec.decode(audio_chunk);
        // Should handle gracefully
    }
};

/**
 * @brief Test memory allocation failure scenarios
 */
class TestMemoryAllocationFailures : public TestCase {
public:
    TestMemoryAllocationFailures() : TestCase("Memory Allocation Failure Scenarios") {}
    
protected:
    void runTest() override {
        // Test codec creation with various configurations that might stress memory
        
        // Test with maximum reasonable channel count
        {
            StreamInfo high_channel_info = createOpusStreamInfo(8); // 7.1 surround
            OpusCodec high_channel_codec(high_channel_info);
            
            ASSERT_TRUE(high_channel_codec.initialize(), "High channel codec should initialize");
            
            auto head_packet = createValidOpusHeadPacket(8);
            MediaChunk head_chunk;
            head_chunk.data = head_packet;
            
            AudioFrame frame = high_channel_codec.decode(head_chunk);
            // Should handle without excessive memory usage
        }
        
        // Test with multiple codec instances (simulate memory pressure)
        {
            std::vector<std::unique_ptr<OpusCodec>> codecs;
            
            for (int i = 0; i < 10; i++) {
                StreamInfo info = createOpusStreamInfo(2);
                auto codec = std::make_unique<OpusCodec>(info);
                
                ASSERT_TRUE(codec->initialize(), "Multiple codec instances should initialize");
                codecs.push_back(std::move(codec));
            }
            
            // All codecs should be functional
            for (auto& codec : codecs) {
                auto head_packet = createValidOpusHeadPacket(2);
                MediaChunk head_chunk;
                head_chunk.data = head_packet;
                
                AudioFrame frame = codec->decode(head_chunk);
                // Should work without memory issues
            }
        }
        
        // Test rapid allocation/deallocation
        {
            for (int i = 0; i < 50; i++) {
                StreamInfo info = createOpusStreamInfo(2);
                OpusCodec codec(info);
                
                ASSERT_TRUE(codec.initialize(), "Rapid allocation codec should initialize");
                
                auto head_packet = createValidOpusHeadPacket(2);
                MediaChunk head_chunk;
                head_chunk.data = head_packet;
                
                AudioFrame frame = codec.decode(head_chunk);
                // Codec destructor should clean up properly
            }
        }
    }
};

/**
 * @brief Test thread safety with concurrent codec instances
 */
class TestThreadSafetyConcurrentInstances : public TestCase {
public:
    TestThreadSafetyConcurrentInstances() : TestCase("Thread Safety with Concurrent Instances") {}
    
protected:
    void runTest() override {
        const int num_threads = 4;
        const int operations_per_thread = 20;
        std::atomic<int> successful_operations{0};
        std::atomic<int> failed_operations{0};
        std::vector<std::thread> threads;
        
        // Create multiple threads, each with its own codec instance
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([&, t]() {
                try {
                    StreamInfo info = createOpusStreamInfo(2);
                    OpusCodec codec(info);
                    
                    if (!codec.initialize()) {
                        failed_operations.fetch_add(1);
                        return;
                    }
                    
                    auto head_packet = createValidOpusHeadPacket(2);
                    auto tags_packet = createValidOpusTagsPacket();
                    
                    for (int op = 0; op < operations_per_thread; op++) {
                        MediaChunk head_chunk;
                        head_chunk.data = head_packet;
                        
                        MediaChunk tags_chunk;
                        tags_chunk.data = tags_packet;
                        
                        // Decode headers
                        AudioFrame head_frame = codec.decode(head_chunk);
                        AudioFrame tags_frame = codec.decode(tags_chunk);
                        
                        // Decode some audio
                        std::vector<uint8_t> audio_packet = {0x00, 0x00};
                        MediaChunk audio_chunk;
                        audio_chunk.data = audio_packet;
                        
                        AudioFrame audio_frame = codec.decode(audio_chunk);
                        
                        // Reset occasionally
                        if (op % 5 == 0) {
                            codec.reset();
                        }
                        
                        successful_operations.fetch_add(1);
                        
                        // Small delay to increase chance of race conditions
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }
                } catch (const std::exception& e) {
                    failed_operations.fetch_add(1);
                } catch (...) {
                    failed_operations.fetch_add(1);
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Verify results
        int expected_operations = num_threads * operations_per_thread;
        ASSERT_EQUALS(expected_operations, successful_operations.load(), 
                     "All operations should succeed in concurrent test");
        ASSERT_EQUALS(0, failed_operations.load(), 
                     "No operations should fail in concurrent test");
    }
};

/**
 * @brief Test error recovery after unrecoverable errors
 */
class TestErrorRecovery : public TestCase {
public:
    TestErrorRecovery() : TestCase("Error Recovery After Unrecoverable Errors") {}
    
protected:
    void runTest() override {
        StreamInfo info = createOpusStreamInfo(2);
        OpusCodec codec(info);
        
        ASSERT_TRUE(codec.initialize(), "Codec initialization should succeed");
        
        // Process valid headers first
        auto head_packet = createValidOpusHeadPacket(2);
        auto tags_packet = createValidOpusTagsPacket();
        
        MediaChunk head_chunk;
        head_chunk.data = head_packet;
        MediaChunk tags_chunk;
        tags_chunk.data = tags_packet;
        
        AudioFrame head_frame = codec.decode(head_chunk);
        AudioFrame tags_frame = codec.decode(tags_chunk);
        
        // Cause multiple errors in sequence
        auto corrupted1 = createCorruptedAudioPacket();
        auto corrupted2 = createCorruptedAudioPacket();
        auto corrupted3 = createCorruptedAudioPacket();
        
        MediaChunk corrupted_chunk1;
        corrupted_chunk1.data = corrupted1;
        MediaChunk corrupted_chunk2;
        corrupted_chunk2.data = corrupted2;
        MediaChunk corrupted_chunk3;
        corrupted_chunk3.data = corrupted3;
        
        // Process multiple corrupted packets
        AudioFrame error_frame1 = codec.decode(corrupted_chunk1);
        AudioFrame error_frame2 = codec.decode(corrupted_chunk2);
        AudioFrame error_frame3 = codec.decode(corrupted_chunk3);
        
        // Try to recover with valid packet
        std::vector<uint8_t> valid_audio = {0x00, 0x00}; // Minimal valid packet
        MediaChunk valid_chunk;
        valid_chunk.data = valid_audio;
        
        AudioFrame recovery_frame = codec.decode(valid_chunk);
        
        // Should be able to recover and continue processing
        if (!recovery_frame.samples.empty()) {
            ASSERT_EQUALS(2u, recovery_frame.channels, "Recovery frame should have correct channels");
            ASSERT_EQUALS(48000u, recovery_frame.sample_rate, "Recovery frame should have correct sample rate");
        }
        
        // Test recovery after reset
        codec.reset();
        
        AudioFrame post_reset_frame = codec.decode(valid_chunk);
        // Should work normally after reset
        
        // Test flush after errors
        AudioFrame flush_frame = codec.flush();
        // Should handle flush gracefully even after errors
    }
};

// ========== Test Suite Setup ==========

int main()
{
    TestSuite suite("Opus Codec Error Handling and Edge Cases Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<TestCorruptedHeaderHandling>());
    suite.addTest(std::make_unique<TestInvalidHeaderParameters>());
    suite.addTest(std::make_unique<TestCorruptedAudioPackets>());
    suite.addTest(std::make_unique<TestDecoderStateReset>());
    suite.addTest(std::make_unique<TestMemoryAllocationFailures>());
    suite.addTest(std::make_unique<TestThreadSafetyConcurrentInstances>());
    suite.addTest(std::make_unique<TestErrorRecovery>());
    
    // Run all tests
    auto results = suite.runAll();
    
    // Print results
    suite.printResults(results);
    
    // Return appropriate exit code
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}

#else

int main()
{
    printf("Opus codec not available (HAVE_OGGDEMUXER not defined)\n");
    return 0;
}

#endif // HAVE_OGGDEMUXER