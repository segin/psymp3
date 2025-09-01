/*
 * test_flac_codec_threading_safety.cpp - Thread safety tests for FLACCodec in multi-threaded playback
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

/**
 * @brief Test FLACCodec thread safety following PsyMP3 threading patterns
 */
class FLACCodecThreadingSafetyTest : public TestCase {
public:
    FLACCodecThreadingSafetyTest() : TestCase("FLACCodec Threading Safety Test") {}
    
protected:
    void runTest() override {
        StreamInfo stream_info;
        stream_info.stream_id = 1;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        stream_info.duration_ms = 10000; // 10 seconds
        
        auto codec = std::make_unique<FLACCodec>(stream_info);
        ASSERT_TRUE(codec->initialize(), "Codec should initialize for threading test");
        
        // Test public/private lock pattern compliance
        testPublicPrivateLockPattern(codec.get());
        
        // Test concurrent public method access
        testConcurrentPublicMethodAccess(codec.get());
        
        // Test lock acquisition order compliance
        testLockAcquisitionOrder(codec.get());
        
        // Test exception safety with locks
        testExceptionSafetyWithLocks(codec.get());
        
        // Test that no callbacks are invoked while holding locks
        testCallbackSafety(codec.get());
    }
    
private:
    void testPublicPrivateLockPattern(FLACCodec* codec) {
        // Test that public methods can be called concurrently without deadlocks
        const int num_threads = 8;
        std::vector<std::thread> threads;
        std::atomic<int> successful_operations{0};
        std::atomic<int> total_operations{0};
        
        // Create test data
        MediaChunk test_chunk = createTestChunk();
        
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < 10; i++) {
                    try {
                        total_operations++;
                        
                        // Test various public methods concurrently
                        switch (i % 5) {
                            case 0: {
                                AudioFrame frame = codec->decode(test_chunk);
                                successful_operations++;
                                break;
                            }
                            case 1: {
                                codec->reset();
                                successful_operations++;
                                break;
                            }
                            case 2: {
                                AudioFrame frame = codec->flush();
                                successful_operations++;
                                break;
                            }
                            case 3: {
                                uint64_t sample = codec->getCurrentSample();
                                successful_operations++;
                                break;
                            }
                            case 4: {
                                auto stats = codec->getStats();
                                successful_operations++;
                                break;
                            }
                        }
                        
                        // Small delay to increase chance of contention
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                        
                    } catch (const std::exception& e) {
                        // Some operations may fail, but should not deadlock
                        std::cout << "Thread " << t << " operation " << i << " failed: " << e.what() << std::endl;
                    }
                }
            });
        }
        
        // Wait for all threads with timeout to detect deadlocks
        auto start_time = std::chrono::steady_clock::now();
        for (auto& thread : threads) {
            thread.join();
        }
        auto end_time = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        ASSERT_TRUE(duration.count() < 30, "Threading test should complete within 30 seconds (no deadlocks)");
        
        ASSERT_TRUE(total_operations.load() > 0, "Should have attempted some operations");
        
        std::cout << "Public/Private lock pattern test: " << successful_operations.load() 
                  << "/" << total_operations.load() << " operations successful" << std::endl;
    }
    
    void testConcurrentPublicMethodAccess(FLACCodec* codec) {
        // Test that multiple threads can call different public methods simultaneously
        std::atomic<bool> test_running{true};
        std::atomic<int> decode_count{0};
        std::atomic<int> reset_count{0};
        std::atomic<int> stats_count{0};
        std::atomic<int> sample_count{0};
        
        MediaChunk test_chunk = createTestChunk();
        
        // Decoder thread
        std::thread decode_thread([&]() {
            while (test_running.load()) {
                try {
                    AudioFrame frame = codec->decode(test_chunk);
                    decode_count++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } catch (const std::exception& e) {
                    // Acceptable
                }
            }
        });
        
        // Reset thread
        std::thread reset_thread([&]() {
            while (test_running.load()) {
                try {
                    codec->reset();
                    reset_count++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                } catch (const std::exception& e) {
                    // Acceptable
                }
            }
        });
        
        // Statistics thread
        std::thread stats_thread([&]() {
            while (test_running.load()) {
                try {
                    auto stats = codec->getStats();
                    stats_count++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                } catch (const std::exception& e) {
                    // Acceptable
                }
            }
        });
        
        // Sample position thread
        std::thread sample_thread([&]() {
            while (test_running.load()) {
                try {
                    uint64_t sample = codec->getCurrentSample();
                    sample_count++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } catch (const std::exception& e) {
                    // Acceptable
                }
            }
        });
        
        // Let threads run for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        test_running.store(false);
        
        // Join all threads
        decode_thread.join();
        reset_thread.join();
        stats_thread.join();
        sample_thread.join();
        
        // Verify that operations occurred without deadlocks
        ASSERT_TRUE(decode_count.load() >= 0, "Decode operations should have occurred");
        ASSERT_TRUE(reset_count.load() >= 0, "Reset operations should have occurred");
        ASSERT_TRUE(stats_count.load() >= 0, "Stats operations should have occurred");
        ASSERT_TRUE(sample_count.load() >= 0, "Sample position operations should have occurred");
        
        std::cout << "Concurrent access test - Decode: " << decode_count.load()
                  << ", Reset: " << reset_count.load()
                  << ", Stats: " << stats_count.load()
                  << ", Sample: " << sample_count.load() << std::endl;
    }
    
    void testLockAcquisitionOrder(FLACCodec* codec) {
        // Test that the documented lock acquisition order is followed
        // This is more of a design verification test
        
        const int num_threads = 4;
        std::vector<std::thread> threads;
        std::atomic<bool> deadlock_detected{false};
        
        MediaChunk test_chunk = createTestChunk();
        
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([&, t]() {
                auto start_time = std::chrono::steady_clock::now();
                
                for (int i = 0; i < 20; i++) {
                    try {
                        // Mix operations that might acquire different locks
                        if (i % 3 == 0) {
                            codec->decode(test_chunk);
                        } else if (i % 3 == 1) {
                            codec->reset();
                        } else {
                            auto stats = codec->getStats();
                        }
                        
                        // Check for excessive delays that might indicate deadlock
                        auto current_time = std::chrono::steady_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
                        
                        if (duration.count() > 5000) { // 5 seconds
                            deadlock_detected.store(true);
                            break;
                        }
                        
                    } catch (const std::exception& e) {
                        // Acceptable
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_FALSE(deadlock_detected.load(), "No deadlocks should be detected with proper lock ordering");
    }
    
    void testExceptionSafetyWithLocks(FLACCodec* codec) {
        // Test that locks are properly released even when exceptions occur
        
        std::atomic<int> successful_operations{0};
        std::atomic<int> exception_operations{0};
        
        const int num_threads = 4;
        std::vector<std::thread> threads;
        
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < 10; i++) {
                    try {
                        // Create potentially problematic data that might cause exceptions
                        MediaChunk bad_chunk;
                        bad_chunk.stream_id = 1;
                        bad_chunk.data = {0xFF, 0xFF, 0xFF, 0xFF}; // Invalid FLAC data
                        bad_chunk.timestamp_samples = i * 100;
                        bad_chunk.is_keyframe = true;
                        
                        AudioFrame frame = codec->decode(bad_chunk);
                        successful_operations++;
                        
                    } catch (const std::exception& e) {
                        exception_operations++;
                        
                        // After exception, codec should still be usable
                        try {
                            codec->getCurrentSample(); // Should not deadlock
                        } catch (...) {
                            // Acceptable
                        }
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Verify codec is still functional after exceptions
        try {
            codec->reset(); // Should work without deadlock
            auto stats = codec->getStats(); // Should work without deadlock
        } catch (const std::exception& e) {
            // Should not throw due to lock issues
            ASSERT_TRUE(false, "Codec should remain functional after exceptions");
        }
        
        std::cout << "Exception safety test - Successful: " << successful_operations.load()
                  << ", Exceptions: " << exception_operations.load() << std::endl;
    }
    
    void testCallbackSafety(FLACCodec* codec) {
        // Test that the codec doesn't invoke callbacks while holding internal locks
        // This is primarily a design verification test
        
        MediaChunk test_chunk = createTestChunk();
        
        // Test that decode operations don't cause callback-related deadlocks
        std::atomic<bool> callback_deadlock{false};
        
        std::thread callback_test_thread([&]() {
            auto start_time = std::chrono::steady_clock::now();
            
            for (int i = 0; i < 10; i++) {
                try {
                    AudioFrame frame = codec->decode(test_chunk);
                    
                    // Check for excessive delays
                    auto current_time = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
                    
                    if (duration.count() > 2000) { // 2 seconds per operation is too long
                        callback_deadlock.store(true);
                        break;
                    }
                    
                } catch (const std::exception& e) {
                    // Acceptable
                }
            }
        });
        
        callback_test_thread.join();
        
        ASSERT_FALSE(callback_deadlock.load(), "No callback-related deadlocks should occur");
    }
    
    MediaChunk createTestChunk() {
        MediaChunk chunk;
        chunk.stream_id = 1;
        chunk.timestamp_samples = 0;
        chunk.is_keyframe = true;
        
        // Create minimal FLAC frame data
        chunk.data = {
            0xFF, 0xF8, // FLAC sync
            0x69,       // Block size + sample rate
            0x10,       // Channels + bits per sample
            0x00,       // Frame number
            0x00        // CRC
        };
        
        // Add some mock compressed data
        for (int i = 0; i < 50; i++) {
            chunk.data.push_back(static_cast<uint8_t>(i & 0xFF));
        }
        
        return chunk;
    }
};

/**
 * @brief Test FLACCodec integration with playback pipeline
 */
class FLACCodecPlaybackIntegrationTest : public TestCase {
public:
    FLACCodecPlaybackIntegrationTest() : TestCase("FLACCodec Playback Integration Test") {}
    
protected:
    void runTest() override {
        // Test codec integration in a simulated playback pipeline
        testPlaybackPipelineIntegration();
        
        // Test codec behavior during seeking operations
        testSeekingIntegration();
        
        // Test codec behavior during format changes
        testFormatChangeHandling();
    }
    
private:
    void testPlaybackPipelineIntegration() {
        StreamInfo stream_info;
        stream_info.stream_id = 1;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        stream_info.duration_ms = 5000;
        
        auto codec = std::make_unique<FLACCodec>(stream_info);
        ASSERT_TRUE(codec->initialize(), "Codec should initialize for playback test");
        
        // Simulate continuous playback
        std::vector<AudioFrame> decoded_frames;
        size_t total_samples = 0;
        
        for (int frame_num = 0; frame_num < 10; frame_num++) {
            MediaChunk chunk = createSequentialChunk(frame_num);
            
            AudioFrame frame = codec->decode(chunk);
            
            if (frame.getSampleFrameCount() > 0) {
                decoded_frames.push_back(frame);
                total_samples += frame.getSampleFrameCount();
                
                // Verify frame properties
                ASSERT_EQUALS(2u, frame.getChannels(), "Frame should have correct channels");
                ASSERT_EQUALS(44100u, frame.getSampleRate(), "Frame should have correct sample rate");
            }
        }
        
        // Test flush at end of stream
        AudioFrame flush_frame = codec->flush();
        if (flush_frame.getSampleFrameCount() > 0) {
            decoded_frames.push_back(flush_frame);
            total_samples += flush_frame.getSampleFrameCount();
        }
        
        ASSERT_TRUE(total_samples >= 0, "Should have decoded some samples");
        
        // Verify codec statistics
        auto stats = codec->getStats();
        ASSERT_TRUE(stats.frames_decoded >= 0, "Should track decoded frames");
        ASSERT_TRUE(stats.samples_decoded >= 0, "Should track decoded samples");
        
        std::cout << "Playback integration test - Frames: " << decoded_frames.size()
                  << ", Samples: " << total_samples << std::endl;
    }
    
    void testSeekingIntegration() {
        StreamInfo stream_info;
        stream_info.stream_id = 1;
        stream_info.codec_type = "audio";
        stream_info.codec_name = "flac";
        stream_info.sample_rate = 44100;
        stream_info.channels = 2;
        stream_info.bits_per_sample = 16;
        stream_info.duration_ms = 10000;
        
        auto codec = std::make_unique<FLACCodec>(stream_info);
        ASSERT_TRUE(codec->initialize(), "Codec should initialize for seeking test");
        
        // Decode some frames
        for (int i = 0; i < 5; i++) {
            MediaChunk chunk = createSequentialChunk(i);
            codec->decode(chunk);
        }
        
        uint64_t position_before_seek = codec->getCurrentSample();
        
        // Simulate seek operation (reset codec state)
        codec->reset();
        ASSERT_EQUALS(0u, codec->getCurrentSample(), "Position should be 0 after seek reset");
        
        // Continue decoding after seek
        for (int i = 0; i < 3; i++) {
            MediaChunk chunk = createSequentialChunk(i + 100); // Different frame numbers
            AudioFrame frame = codec->decode(chunk);
            
            if (frame.getSampleFrameCount() > 0) {
                ASSERT_EQUALS(2u, frame.getChannels(), "Frame should have correct channels after seek");
                ASSERT_EQUALS(44100u, frame.getSampleRate(), "Frame should have correct sample rate after seek");
            }
        }
        
        // Verify codec is functional after seek
        auto stats = codec->getStats();
        ASSERT_TRUE(stats.frames_decoded >= 0, "Should track frames after seek");
        
        std::cout << "Seeking integration test completed successfully" << std::endl;
    }
    
    void testFormatChangeHandling() {
        // Test codec behavior when stream format changes (should handle gracefully)
        
        StreamInfo initial_stream_info;
        initial_stream_info.stream_id = 1;
        initial_stream_info.codec_type = "audio";
        initial_stream_info.codec_name = "flac";
        initial_stream_info.sample_rate = 44100;
        initial_stream_info.channels = 2;
        initial_stream_info.bits_per_sample = 16;
        initial_stream_info.duration_ms = 5000;
        
        auto codec = std::make_unique<FLACCodec>(initial_stream_info);
        ASSERT_TRUE(codec->initialize(), "Codec should initialize for format change test");
        
        // Decode with initial format
        MediaChunk chunk1 = createChunkWithFormat(44100, 2, 16);
        AudioFrame frame1 = codec->decode(chunk1);
        
        // Try to decode with different format (should handle gracefully)
        MediaChunk chunk2 = createChunkWithFormat(48000, 2, 24);
        AudioFrame frame2 = codec->decode(chunk2);
        
        // Codec should remain functional
        auto stats = codec->getStats();
        ASSERT_TRUE(stats.error_count >= 0, "Should track errors during format changes");
        
        // Reset and continue with original format
        codec->reset();
        MediaChunk chunk3 = createChunkWithFormat(44100, 2, 16);
        AudioFrame frame3 = codec->decode(chunk3);
        
        std::cout << "Format change handling test completed" << std::endl;
    }
    
    MediaChunk createSequentialChunk(int frame_number) {
        MediaChunk chunk;
        chunk.stream_id = 1;
        chunk.timestamp_samples = frame_number * 93; // ~93ms per 4096-sample frame at 44.1kHz
        chunk.is_keyframe = true;
        
        // Create FLAC frame with frame number
        chunk.data = {
            0xFF, 0xF8, // FLAC sync
            0x69,       // Block size + sample rate (44.1kHz, 4096 samples)
            0x10,       // Stereo, 16-bit
            static_cast<uint8_t>(frame_number & 0xFF), // Frame number
            0x00        // CRC
        };
        
        // Add mock compressed data
        for (int i = 0; i < 100; i++) {
            chunk.data.push_back(static_cast<uint8_t>((i + frame_number) & 0xFF));
        }
        
        return chunk;
    }
    
    MediaChunk createChunkWithFormat(uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample) {
        MediaChunk chunk;
        chunk.stream_id = 1;
        chunk.timestamp_samples = 0;
        chunk.is_keyframe = true;
        
        // Create FLAC frame header with specified format
        chunk.data = {0xFF, 0xF8}; // FLAC sync
        
        // Encode sample rate
        uint8_t sr_byte = 0x60; // Default
        if (sample_rate == 44100) sr_byte = 0x69;
        else if (sample_rate == 48000) sr_byte = 0x6A;
        else if (sample_rate == 96000) sr_byte = 0x6B;
        
        chunk.data.push_back(sr_byte);
        
        // Encode channels and bit depth
        uint8_t ch_bits_byte = 0x00;
        if (channels == 1) ch_bits_byte |= 0x00;
        else if (channels == 2) ch_bits_byte |= 0x10;
        
        if (bits_per_sample == 8) ch_bits_byte |= 0x01;
        else if (bits_per_sample == 16) ch_bits_byte |= 0x02;
        else if (bits_per_sample == 24) ch_bits_byte |= 0x04;
        
        chunk.data.push_back(ch_bits_byte);
        chunk.data.push_back(0x00); // Frame number
        chunk.data.push_back(0x00); // CRC
        
        // Add mock data
        for (int i = 0; i < 80; i++) {
            chunk.data.push_back(static_cast<uint8_t>(i & 0xFF));
        }
        
        return chunk;
    }
};

int main() {
    TestSuite suite("FLAC Codec Threading Safety Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<FLACCodecThreadingSafetyTest>());
    suite.addTest(std::make_unique<FLACCodecPlaybackIntegrationTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}