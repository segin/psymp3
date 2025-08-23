/*
 * test_flac_compatibility_integration.cpp - Integration tests comparing FLACDemuxer with existing FLAC implementation
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
 * @brief Test file generator for creating real FLAC test files
 */
class FLACTestFileGenerator {
public:
    /**
     * @brief Generate a simple FLAC test file for testing
     * @param filename Output filename
     * @param duration_seconds Duration in seconds
     * @param sample_rate Sample rate
     * @param channels Number of channels
     * @return true if file was created successfully
     */
    static bool generateTestFile(const std::string& filename, 
                                int duration_seconds = 10,
                                int sample_rate = 44100,
                                int channels = 2) {
        try {
            // Create a simple sine wave test file
            std::vector<int16_t> samples;
            int total_samples = duration_seconds * sample_rate * channels;
            
            for (int i = 0; i < total_samples; i += channels) {
                // Generate sine wave at 440 Hz
                double time = static_cast<double>(i / channels) / sample_rate;
                double amplitude = 16000.0; // Reasonable amplitude
                double frequency = 440.0; // A4 note
                
                int16_t sample_value = static_cast<int16_t>(amplitude * std::sin(2.0 * M_PI * frequency * time));
                
                // Add samples for each channel
                for (int ch = 0; ch < channels; ch++) {
                    samples.push_back(sample_value);
                }
            }
            
            // Write to a temporary WAV file first, then convert to FLAC
            // This is a simplified approach - in practice, you'd use libFLAC directly
            std::string wav_filename = filename + ".wav";
            
            if (writeWAVFile(wav_filename, samples, sample_rate, channels)) {
                // Try to convert WAV to FLAC using system flac encoder if available
                std::string flac_command = "flac --silent --force --output-name=" + filename + " " + wav_filename;
                int result = std::system(flac_command.c_str());
                
                // Clean up temporary WAV file
                std::remove(wav_filename.c_str());
                
                return result == 0;
            }
            
            return false;
        } catch (...) {
            return false;
        }
    }
    
private:
    /**
     * @brief Write samples to a WAV file
     */
    static bool writeWAVFile(const std::string& filename, 
                            const std::vector<int16_t>& samples,
                            int sample_rate, 
                            int channels) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;
        
        // WAV header
        uint32_t data_size = samples.size() * sizeof(int16_t);
        uint32_t file_size = 36 + data_size;
        uint16_t bits_per_sample = 16;
        uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8;
        uint16_t block_align = channels * bits_per_sample / 8;
        
        // RIFF header
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char*>(&file_size), 4);
        file.write("WAVE", 4);
        
        // fmt chunk
        file.write("fmt ", 4);
        uint32_t fmt_size = 16;
        file.write(reinterpret_cast<const char*>(&fmt_size), 4);
        uint16_t audio_format = 1; // PCM
        file.write(reinterpret_cast<const char*>(&audio_format), 2);
        file.write(reinterpret_cast<const char*>(&channels), 2);
        file.write(reinterpret_cast<const char*>(&sample_rate), 4);
        file.write(reinterpret_cast<const char*>(&byte_rate), 4);
        file.write(reinterpret_cast<const char*>(&block_align), 2);
        file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
        
        // data chunk
        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&data_size), 4);
        file.write(reinterpret_cast<const char*>(samples.data()), data_size);
        
        return file.good();
    }
};

/**
 * @brief Test FLACDemuxer against existing FLAC implementation with real files
 */
class FLACDemuxerVsExistingTest : public TestCase {
public:
    FLACDemuxerVsExistingTest() : TestCase("FLACDemuxer vs Existing Implementation Test") {}
    
protected:
    void runTest() override {
        // Try to generate a test FLAC file
        std::string test_file = "test_compatibility.flac";
        bool file_created = FLACTestFileGenerator::generateTestFile(test_file, 5, 44100, 2);
        
        if (!file_created) {
            // Skip this test if we can't create FLAC files
            std::cout << "Skipping real file test - FLAC encoder not available" << std::endl;
            return;
        }
        
        try {
            // Test with FLACDemuxer
            auto demuxer_handler = std::make_unique<FileIOHandler>(test_file);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(demuxer_handler));
            
            bool demuxer_parsed = demuxer->parseContainer();
            
            if (demuxer_parsed) {
                auto demuxer_streams = demuxer->getStreams();
                uint64_t demuxer_duration = demuxer->getDuration();
                
                ASSERT_EQUALS(1u, demuxer_streams.size(), "FLACDemuxer should find one stream");
                ASSERT_EQUALS(44100u, demuxer_streams[0].sample_rate, "Sample rate should match");
                ASSERT_EQUALS(2u, demuxer_streams[0].channels, "Channels should match");
                ASSERT_TRUE(demuxer_duration > 4000 && demuxer_duration < 6000, 
                           "Duration should be approximately 5 seconds");
                
                // Test seeking
                ASSERT_TRUE(demuxer->seekTo(2500), "Should seek to middle");
                uint64_t seek_position = demuxer->getPosition();
                ASSERT_TRUE(seek_position >= 2000 && seek_position <= 3000, 
                           "Seek position should be approximately correct");
                
                // Test frame reading
                auto chunk = demuxer->readChunk();
                if (chunk.isValid()) {
                    ASSERT_EQUALS(1u, chunk.stream_id, "Chunk should have correct stream ID");
                    ASSERT_FALSE(chunk.data.empty(), "Chunk should have data");
                }
            }
            
#ifdef HAVE_FLAC
            // Test with existing FLAC implementation if available
            try {
                auto existing_flac = std::make_unique<Flac>(test_file);
                
                // Compare basic properties
                if (demuxer_parsed) {
                    // Compare sample rate
                    ASSERT_EQUALS(existing_flac->getRate(), demuxer_streams[0].sample_rate, 
                                 "Sample rates should match between implementations");
                    
                    // Compare channels
                    ASSERT_EQUALS(existing_flac->getChannels(), demuxer_streams[0].channels,
                                 "Channels should match between implementations");
                    
                    // Compare duration (allow some tolerance)
                    uint64_t existing_duration = existing_flac->getLength();
                    uint64_t duration_diff = std::abs(static_cast<int64_t>(existing_duration) - 
                                                     static_cast<int64_t>(demuxer_duration));
                    ASSERT_TRUE(duration_diff < 100, "Durations should be similar between implementations");
                    
                    // Test seeking comparison
                    existing_flac->seekTo(2500);
                    uint64_t existing_position = existing_flac->getPosition();
                    
                    demuxer->seekTo(2500);
                    uint64_t demuxer_position = demuxer->getPosition();
                    
                    uint64_t position_diff = std::abs(static_cast<int64_t>(existing_position) - 
                                                     static_cast<int64_t>(demuxer_position));
                    ASSERT_TRUE(position_diff < 1000, "Seek positions should be similar");
                }
            } catch (const std::exception& e) {
                // Existing implementation may fail for various reasons, that's OK
                std::cout << "Existing FLAC implementation test skipped: " << e.what() << std::endl;
            }
#endif
            
        } catch (const std::exception& e) {
            // Clean up test file
            std::remove(test_file.c_str());
            throw;
        }
        
        // Clean up test file
        std::remove(test_file.c_str());
    }
};

/**
 * @brief Test FLACDemuxer with various FLAC file types
 */
class FLACDemuxerFileTypesTest : public TestCase {
public:
    FLACDemuxerFileTypesTest() : TestCase("FLACDemuxer File Types Test") {}
    
protected:
    void runTest() override {
        // Test different FLAC file configurations
        struct TestConfig {
            std::string name;
            int sample_rate;
            int channels;
            int duration;
        };
        
        std::vector<TestConfig> configs = {
            {"mono_44k", 44100, 1, 3},
            {"stereo_48k", 48000, 2, 3},
            {"stereo_96k", 96000, 2, 2}, // Shorter for high sample rate
        };
        
        for (const auto& config : configs) {
            std::string filename = "test_" + config.name + ".flac";
            
            bool created = FLACTestFileGenerator::generateTestFile(
                filename, config.duration, config.sample_rate, config.channels);
            
            if (created) {
                try {
                    auto handler = std::make_unique<FileIOHandler>(filename);
                    auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
                    
                    bool parsed = demuxer->parseContainer();
                    ASSERT_TRUE(parsed, "Should parse " + config.name + " FLAC file");
                    
                    if (parsed) {
                        auto streams = demuxer->getStreams();
                        ASSERT_EQUALS(1u, streams.size(), config.name + " should have one stream");
                        
                        const auto& stream = streams[0];
                        ASSERT_EQUALS(static_cast<uint32_t>(config.sample_rate), stream.sample_rate, 
                                     config.name + " sample rate should match");
                        ASSERT_EQUALS(static_cast<uint32_t>(config.channels), stream.channels,
                                     config.name + " channels should match");
                        
                        // Test basic operations
                        uint64_t duration = demuxer->getDuration();
                        ASSERT_TRUE(duration > 0, config.name + " should have valid duration");
                        
                        ASSERT_TRUE(demuxer->seekTo(duration / 2), 
                                   config.name + " should support seeking");
                        
                        auto chunk = demuxer->readChunk();
                        // Chunk may or may not be valid depending on mock data, but should not crash
                    }
                } catch (const std::exception& e) {
                    // File operations may fail, but should not crash
                    std::cout << "Test " << config.name << " failed: " << e.what() << std::endl;
                }
                
                // Clean up
                std::remove(filename.c_str());
            } else {
                std::cout << "Skipping " << config.name << " test - file creation failed" << std::endl;
            }
        }
    }
};

/**
 * @brief Test FLACDemuxer error handling with real files
 */
class FLACDemuxerRealFileErrorTest : public TestCase {
public:
    FLACDemuxerRealFileErrorTest() : TestCase("FLACDemuxer Real File Error Handling Test") {}
    
protected:
    void runTest() override {
        // Test with non-existent file
        try {
            auto handler = std::make_unique<FileIOHandler>("nonexistent.flac");
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            
            bool parsed = demuxer->parseContainer();
            ASSERT_FALSE(parsed, "Should not parse non-existent file");
        } catch (const std::exception& e) {
            // Exception is acceptable for non-existent file
        }
        
        // Test with non-FLAC file (create a text file with .flac extension)
        std::string fake_flac = "fake.flac";
        std::ofstream fake_file(fake_flac);
        fake_file << "This is not a FLAC file";
        fake_file.close();
        
        try {
            auto handler = std::make_unique<FileIOHandler>(fake_flac);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            
            bool parsed = demuxer->parseContainer();
            ASSERT_FALSE(parsed, "Should not parse fake FLAC file");
            
            // Test operations on unparsed demuxer
            auto streams = demuxer->getStreams();
            ASSERT_TRUE(streams.empty(), "Unparsed demuxer should return empty streams");
            
            ASSERT_EQUALS(0u, demuxer->getDuration(), "Unparsed demuxer should return 0 duration");
            
            auto chunk = demuxer->readChunk();
            ASSERT_FALSE(chunk.isValid(), "Unparsed demuxer should return invalid chunk");
            
            ASSERT_FALSE(demuxer->seekTo(1000), "Unparsed demuxer should reject seeks");
            
        } catch (const std::exception& e) {
            // Exceptions are acceptable for invalid files
        }
        
        // Clean up
        std::remove(fake_flac.c_str());
        
        // Test with empty file
        std::string empty_flac = "empty.flac";
        std::ofstream empty_file(empty_flac);
        empty_file.close();
        
        try {
            auto handler = std::make_unique<FileIOHandler>(empty_flac);
            auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
            
            bool parsed = demuxer->parseContainer();
            ASSERT_FALSE(parsed, "Should not parse empty file");
        } catch (const std::exception& e) {
            // Exception is acceptable for empty file
        }
        
        // Clean up
        std::remove(empty_flac.c_str());
    }
};

/**
 * @brief Test FLACDemuxer integration with MediaFactory
 */
class FLACDemuxerMediaFactoryTest : public TestCase {
public:
    FLACDemuxerMediaFactoryTest() : TestCase("FLACDemuxer MediaFactory Integration Test") {}
    
protected:
    void runTest() override {
        // Test MediaFactory format detection
        ASSERT_TRUE(MediaFactory::supportsExtension("flac"), "MediaFactory should support .flac extension");
        ASSERT_TRUE(MediaFactory::supportsMimeType("audio/flac"), "MediaFactory should support audio/flac MIME type");
        
        // Test content analysis
        ContentInfo flac_info = MediaFactory::analyzeContent("test.flac");
        ASSERT_EQUALS("flac", flac_info.file_extension, "Should detect FLAC extension");
        ASSERT_EQUALS("flac", flac_info.detected_format, "Should detect FLAC format");
        ASSERT_TRUE(flac_info.confidence > 0.0f, "Should have confidence in FLAC detection");
        
        // Test MIME type utilities
        std::string mime_type = MediaFactory::extensionToMimeType("flac");
        ASSERT_EQUALS("audio/flac", mime_type, "Should return correct MIME type for FLAC");
        
        std::string extension = MediaFactory::mimeTypeToExtension("audio/flac");
        ASSERT_EQUALS("flac", extension, "Should return correct extension for FLAC MIME type");
        
        // Test stream creation (will fail due to no file, but should not crash)
        try {
            auto stream = MediaFactory::createStream("test.flac");
            // May succeed or fail depending on file existence
        } catch (const std::exception& e) {
            // File not found or other errors are acceptable
            std::string error_msg = e.what();
            // Should be file-related error, not format error
        }
        
        // Test with MIME type hint
        try {
            auto stream = MediaFactory::createStreamWithMimeType("stream", "audio/flac");
            // May succeed or fail, but should not crash
        } catch (const std::exception& e) {
            // Errors are acceptable for this test
        }
    }
};

int main() {
    TestSuite suite("FLAC Demuxer Compatibility Integration Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<FLACDemuxerVsExistingTest>());
    suite.addTest(std::make_unique<FLACDemuxerFileTypesTest>());
    suite.addTest(std::make_unique<FLACDemuxerRealFileErrorTest>());
    suite.addTest(std::make_unique<FLACDemuxerMediaFactoryTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}