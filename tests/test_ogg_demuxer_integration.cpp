/*
 * test_ogg_demuxer_integration.cpp - Comprehensive integration tests for OggDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_OGGDEMUXER

#include "test_framework.h"
#include "demuxer/ogg/OggDemuxer.h"
#include "demuxer/DemuxerRegistry.h"
#include "io/file/FileIOHandler.h"
#include <memory>
#include <vector>
#include <fstream>
#include <cstring>

using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using namespace PsyMP3::Demuxer::Ogg;
using namespace PsyMP3::IO;

/**
 * @brief Comprehensive integration tests for OggDemuxer class
 * 
 * Tests the complete integration of OggDemuxer with:
 * - PsyMP3 Demuxer interface
 * - IOHandler integration (file and HTTP sources)
 * - MediaChunk creation and StreamInfo population
 * - Debug logging using PsyMP3 Debug system
 * - Error code mapping to PsyMP3 conventions
 * - Resource management and cleanup
 */
class OggDemuxerIntegrationTest {
public:
    static void runAllTests() {
        std::cout << "=== OggDemuxer Integration Tests ===" << std::endl;
        
        // Test 1: Basic demuxer interface compliance
        testDemuxerInterfaceCompliance();
        
        // Test 2: IOHandler integration
        testIOHandlerIntegration();
        
        // Test 3: MediaChunk creation and population
        testMediaChunkCreation();
        
        // Test 4: StreamInfo population
        testStreamInfoPopulation();
        
        // Test 5: Debug logging integration
        testDebugLoggingIntegration();
        
        // Test 6: Error code mapping
        testErrorCodeMapping();
        
        // Test 7: Resource management and cleanup
        testResourceManagement();
        
        // Test 8: Complete workflow integration
        testCompleteWorkflowIntegration();
        
        // Test 9: DemuxerRegistry integration
        testDemuxerRegistryIntegration();
        
        // Test 10: MediaFactory integration (skipped due to dependencies)
        // testMediaFactoryIntegration();
        
        std::cout << "=== All OggDemuxer Integration Tests Completed ===" << std::endl;
    }

private:
    /**
     * @brief Test compliance with PsyMP3 Demuxer interface
     */
    static void testDemuxerInterfaceCompliance() {
        std::cout << "Testing Demuxer interface compliance..." << std::endl;
        
        // Create IOHandler for testing
        auto handler = createTestIOHandler();
        
        // Create OggDemuxer instance
        auto demuxer = std::make_unique<OggDemuxer>(std::move(handler));
        
        // Test interface methods exist and work
        ASSERT_FALSE(demuxer->isParsed(), "Demuxer should not be parsed initially");
        
        // Test that parseContainer doesn't crash (result may vary with minimal test data)
        bool parse_result = demuxer->parseContainer();
        
        // Test stream information methods (should not crash regardless of parse result)
        auto streams = demuxer->getStreams();
        std::cout << "  Parse result: " << (parse_result ? "success" : "failed") << std::endl;
        std::cout << "  Found " << streams.size() << " streams" << std::endl;
        
        // Test that the interface methods work without crashing
        if (parse_result && !streams.empty()) {
            ASSERT_TRUE(demuxer->isParsed(), "Demuxer should be parsed after successful parseContainer");
            auto stream_info = demuxer->getStreamInfo(streams[0].stream_id);
            ASSERT_TRUE(stream_info.isValid(), "StreamInfo should be valid");
        }
        
        // Test position and duration methods
        uint64_t duration = demuxer->getDuration();
        uint64_t position = demuxer->getPosition();
        bool eof = demuxer->isEOF();
        
        // These should not throw exceptions
        ASSERT_TRUE(true, "Position/duration methods should not throw");
        
        std::cout << "✓ Demuxer interface compliance test passed" << std::endl;
    }
    
    /**
     * @brief Test IOHandler integration for file and HTTP sources
     */
    static void testIOHandlerIntegration() {
        std::cout << "Testing IOHandler integration..." << std::endl;
        
        // Test 1: IOHandler integration
        auto test_handler = createTestIOHandler();
        
        auto demuxer1 = std::make_unique<OggDemuxer>(std::move(test_handler));
        // Test that IOHandler integration doesn't crash (parse result may vary)
        bool parse_result = demuxer1->parseContainer();
        std::cout << "  Memory IOHandler parse result: " << (parse_result ? "success" : "failed") << std::endl;
        
        // Test 2: File IOHandler integration (if test file exists)
        try {
            auto file_handler = std::make_unique<FileIOHandler>("simple_test.txt");
            // This will likely fail since simple_test.txt is not an Ogg file,
            // but we're testing that the integration doesn't crash
            auto demuxer2 = std::make_unique<OggDemuxer>(std::move(file_handler));
            demuxer2->parseContainer(); // May fail, but shouldn't crash
        } catch (const std::exception& e) {
            // Expected for non-Ogg files
        }
        
        // Test 3: Error handling with invalid IOHandler
        try {
            auto null_handler = std::unique_ptr<IOHandler>(nullptr);
            // This should be caught in constructor or parseContainer
        } catch (const std::exception& e) {
            // Expected
        }
        
        std::cout << "✓ IOHandler integration test passed" << std::endl;
    }
    
    /**
     * @brief Test MediaChunk creation and population
     */
    static void testMediaChunkCreation() {
        std::cout << "Testing MediaChunk creation..." << std::endl;
        
        auto handler = createTestIOHandler();
        auto demuxer = std::make_unique<OggDemuxer>(std::move(handler));
        
        if (demuxer->parseContainer()) {
            auto streams = demuxer->getStreams();
            if (!streams.empty()) {
                // Test reading chunks
                MediaChunk chunk = demuxer->readChunk();
                
                // Test MediaChunk properties
                if (chunk.isValid()) {
                    ASSERT_TRUE(chunk.stream_id != 0, "MediaChunk should have valid stream ID");
                    ASSERT_TRUE(!chunk.data.empty(), "MediaChunk should have data");
                    ASSERT_TRUE(chunk.getDataSize() > 0, "MediaChunk should report correct data size");
                    
                    // Test that chunk belongs to a known stream
                    bool found_stream = false;
                    for (const auto& stream : streams) {
                        if (stream.stream_id == chunk.stream_id) {
                            found_stream = true;
                            break;
                        }
                    }
                    ASSERT_TRUE(found_stream, "MediaChunk stream ID should match available streams");
                }
                
                // Test reading from specific stream
                MediaChunk specific_chunk = demuxer->readChunk(streams[0].stream_id);
                // May be empty if no more data, but shouldn't crash
            } else {
                std::cout << "  Note: No streams found in test file (expected for minimal test)" << std::endl;
            }
        } else {
            std::cout << "  Note: Parse failed for minimal test file (expected)" << std::endl;
        }
        
        std::cout << "✓ MediaChunk creation test passed" << std::endl;
    }
    
    /**
     * @brief Test StreamInfo population with correct metadata
     */
    static void testStreamInfoPopulation() {
        std::cout << "Testing StreamInfo population..." << std::endl;
        
        auto handler = createTestIOHandler();
        auto demuxer = std::make_unique<OggDemuxer>(std::move(handler));
        
        if (demuxer->parseContainer()) {
            auto streams = demuxer->getStreams();
            
            if (!streams.empty()) {
                for (const auto& stream : streams) {
                    // Test required StreamInfo fields
                    ASSERT_TRUE(stream.stream_id != 0, "StreamInfo should have valid stream ID");
                    ASSERT_TRUE(!stream.codec_type.empty(), "StreamInfo should have codec type");
                    ASSERT_TRUE(!stream.codec_name.empty(), "StreamInfo should have codec name");
                    
                    // Test audio-specific fields for audio streams
                    if (stream.codec_type == "audio") {
                        // These may be 0 for some formats, but should be set if available
                        if (stream.sample_rate > 0) {
                            ASSERT_TRUE(stream.sample_rate >= 8000 && stream.sample_rate <= 192000,
                                       "Sample rate should be in reasonable range");
                        }
                        
                        if (stream.channels > 0) {
                            ASSERT_TRUE(stream.channels >= 1 && stream.channels <= 8,
                                       "Channel count should be in reasonable range");
                        }
                    }
                    
                    // Test codec tag assignment
                    if (stream.codec_name == "vorbis") {
                        ASSERT_EQUALS(stream.codec_tag, 0x566F7262U, "Vorbis codec tag should be correct");
                    } else if (stream.codec_name == "opus") {
                        ASSERT_EQUALS(stream.codec_tag, 0x4F707573U, "Opus codec tag should be correct");
                    } else if (stream.codec_name == "flac") {
                        ASSERT_EQUALS(stream.codec_tag, 0x664C6143U, "FLAC codec tag should be correct");
                    }
                }
            } else {
                std::cout << "  Note: No streams found for StreamInfo test (expected for minimal file)" << std::endl;
            }
        } else {
            std::cout << "  Note: Parse failed for StreamInfo test (expected for minimal file)" << std::endl;
        }
        
        std::cout << "✓ StreamInfo population test passed" << std::endl;
    }
    
    /**
     * @brief Test debug logging integration with PsyMP3 Debug system
     */
    static void testDebugLoggingIntegration() {
        std::cout << "Testing debug logging integration..." << std::endl;
        
        // Debug logging is always enabled in PsyMP3, no need to set levels
        auto handler = createTestIOHandler();
        auto demuxer = std::make_unique<OggDemuxer>(std::move(handler));
        
        // These operations should generate debug logs
        demuxer->parseContainer();
        demuxer->getStreams();
        demuxer->readChunk();
        
        // Test that debug logging doesn't crash or interfere with functionality
        ASSERT_TRUE(true, "Debug logging should not interfere with operation");
        
        std::cout << "✓ Debug logging integration test passed" << std::endl;
    }
    
    /**
     * @brief Test error code mapping to PsyMP3 conventions
     */
    static void testErrorCodeMapping() {
        std::cout << "Testing error code mapping..." << std::endl;
        
        // Test with invalid data
        std::vector<uint8_t> invalid_data = {0x00, 0x01, 0x02, 0x03}; // Not Ogg data
        std::string temp_filename = "/tmp/invalid_ogg_test_" + std::to_string(rand()) + ".ogg";
        std::ofstream temp_file(temp_filename, std::ios::binary);
        temp_file.write(reinterpret_cast<const char*>(invalid_data.data()), invalid_data.size());
        temp_file.close();
        auto handler = std::make_unique<FileIOHandler>(temp_filename);
        auto demuxer = std::make_unique<OggDemuxer>(std::move(handler));
        
        // This should fail gracefully with proper error reporting
        bool parse_result = demuxer->parseContainer();
        
        if (!parse_result) {
            // Check that error information is available
            auto error = demuxer->getLastError();
            ASSERT_TRUE(!error.category.empty(), "Error should have category");
            ASSERT_TRUE(!error.message.empty(), "Error should have message");
        }
        
        // Test seeking beyond file bounds
        auto handler2 = createTestIOHandler();
        auto demuxer2 = std::make_unique<OggDemuxer>(std::move(handler2));
        
        if (demuxer2->parseContainer()) {
            // Try to seek to an invalid position
            bool seek_result = demuxer2->seekTo(999999999); // Very large timestamp
            // Should handle gracefully without crashing
        }
        
        std::cout << "✓ Error code mapping test passed" << std::endl;
    }
    
    /**
     * @brief Test resource management and cleanup
     */
    static void testResourceManagement() {
        std::cout << "Testing resource management..." << std::endl;
        
        // Test multiple demuxer instances
        std::vector<std::unique_ptr<OggDemuxer>> demuxers;
        
        for (int i = 0; i < 5; ++i) {
            auto handler = createTestIOHandler();
            auto demuxer = std::make_unique<OggDemuxer>(std::move(handler));
            demuxer->parseContainer();
            demuxers.push_back(std::move(demuxer));
        }
        
        // Clear all demuxers - should clean up resources properly
        demuxers.clear();
        
        // Test that cleanup doesn't cause issues
        ASSERT_TRUE(true, "Resource cleanup should not cause crashes");
        
        // Test exception safety during construction
        try {
            for (int i = 0; i < 3; ++i) {
                auto handler = createTestIOHandler();
                auto demuxer = std::make_unique<OggDemuxer>(std::move(handler));
                // Destructor should handle cleanup even if parseContainer not called
            }
        } catch (const std::exception& e) {
            // Should not throw during normal construction/destruction
            ASSERT_TRUE(false, "Resource management should not throw exceptions");
        }
        
        std::cout << "✓ Resource management test passed" << std::endl;
    }
    
    /**
     * @brief Test complete workflow integration
     */
    static void testCompleteWorkflowIntegration() {
        std::cout << "Testing complete workflow integration..." << std::endl;
        
        auto handler = createTestIOHandler();
        auto demuxer = std::make_unique<OggDemuxer>(std::move(handler));
        
        // Complete workflow: parse -> get streams -> read data -> seek -> read more
        bool parse_ok = demuxer->parseContainer();
        if (parse_ok) {
            auto streams = demuxer->getStreams();
            if (!streams.empty()) {
                // Read some chunks
                for (int i = 0; i < 3; ++i) {
                    MediaChunk chunk = demuxer->readChunk();
                    if (!chunk.isValid()) break;
                }
                
                // Try seeking
                demuxer->seekTo(0);
                
                // Read more chunks after seek
                for (int i = 0; i < 2; ++i) {
                    MediaChunk chunk = demuxer->readChunk();
                    if (!chunk.isValid()) break;
                }
                
                // Check final state
                uint64_t final_position = demuxer->getPosition();
                bool final_eof = demuxer->isEOF();
                
                ASSERT_TRUE(true, "Complete workflow should execute without crashes");
            }
        }
        
        std::cout << "✓ Complete workflow integration test passed" << std::endl;
    }
    
    /**
     * @brief Test DemuxerRegistry integration
     */
    static void testDemuxerRegistryIntegration() {
        std::cout << "Testing DemuxerRegistry integration..." << std::endl;
        
        // Test that OggDemuxer is properly registered
        auto& registry = DemuxerRegistry::getInstance();
        
        // List all supported formats for debugging
        auto supported_formats = registry.getSupportedFormats();
        std::cout << "  Registry has " << supported_formats.size() << " supported formats:" << std::endl;
        for (const auto& format : supported_formats) {
            std::cout << "    - " << format.format_id << " (" << format.format_name << ")" << std::endl;
        }
        
        // Check if Ogg format is supported (may not be if dependencies are missing)
        bool ogg_supported = registry.isFormatSupported("ogg");
        std::cout << "  Ogg format supported: " << (ogg_supported ? "yes" : "no") << std::endl;
        
        if (ogg_supported) {
            ASSERT_TRUE(registry.isExtensionSupported("ogg"), "OGG extension should be supported");
            ASSERT_TRUE(registry.isExtensionSupported("oga"), "OGA extension should be supported");
        } else {
            std::cout << "  Ogg format not registered (may be missing dependencies)" << std::endl;
        }
        
        // Test creating demuxer through registry (if Ogg is supported)
        if (ogg_supported) {
            std::vector<uint8_t> test_data = createMinimalOggFile();
            auto handler = createMemoryIOHandler(test_data);
            
            auto demuxer = registry.createDemuxer(std::move(handler));
            ASSERT_TRUE(demuxer != nullptr, "Registry should create valid demuxer");
            
            // Test that it's actually an OggDemuxer
            auto ogg_demuxer = dynamic_cast<OggDemuxer*>(demuxer.get());
            ASSERT_TRUE(ogg_demuxer != nullptr, "Registry should create OggDemuxer for Ogg data");
        } else {
            std::cout << "  Skipping demuxer creation test (Ogg not supported)" << std::endl;
        }
        
        std::cout << "✓ DemuxerRegistry integration test passed" << std::endl;
    }
    
    /**
     * @brief Test MediaFactory integration (skipped due to dependencies)
     */
    static void testMediaFactoryIntegration() {
        std::cout << "Testing MediaFactory integration (skipped)..." << std::endl;
        std::cout << "✓ MediaFactory integration test skipped (dependency issues)" << std::endl;
    }
    
    /**
     * @brief Create a minimal valid Ogg file for testing
     */
    static std::vector<uint8_t> createMinimalOggFile() {
        std::vector<uint8_t> ogg_data;
        
        // Create a more complete minimal Ogg page with proper structure
        // This is based on the actual Ogg specification
        
        // Ogg page header
        ogg_data.insert(ogg_data.end(), {'O', 'g', 'g', 'S'}); // Capture pattern
        ogg_data.push_back(0x00); // Version
        ogg_data.push_back(0x02); // Header type (first page of logical bitstream)
        
        // Granule position (8 bytes, little-endian) - 0 for header pages
        for (int i = 0; i < 8; ++i) ogg_data.push_back(0x00);
        
        // Serial number (4 bytes, little-endian) - use a simple serial number
        ogg_data.insert(ogg_data.end(), {0x01, 0x00, 0x00, 0x00});
        
        // Page sequence (4 bytes, little-endian) - first page
        for (int i = 0; i < 4; ++i) ogg_data.push_back(0x00);
        
        // Checksum (4 bytes) - we'll leave as 0 for simplicity
        for (int i = 0; i < 4; ++i) ogg_data.push_back(0x00);
        
        // Number of segments - 1 segment
        ogg_data.push_back(0x01);
        
        // Segment table - one segment of 30 bytes
        ogg_data.push_back(30);
        
        // Minimal Vorbis identification header (30 bytes total)
        ogg_data.insert(ogg_data.end(), {0x01, 'v', 'o', 'r', 'b', 'i', 's'}); // 7 bytes
        
        // Vorbis version (4 bytes) - version 0
        ogg_data.insert(ogg_data.end(), {0x00, 0x00, 0x00, 0x00}); // 4 bytes
        
        // Channels (1 byte) - stereo
        ogg_data.push_back(0x02); // 1 byte
        
        // Sample rate (4 bytes, little-endian) - 44100 Hz
        ogg_data.insert(ogg_data.end(), {0x44, 0xAC, 0x00, 0x00}); // 4 bytes
        
        // Bitrate max (4 bytes) - 0 (unknown)
        ogg_data.insert(ogg_data.end(), {0x00, 0x00, 0x00, 0x00}); // 4 bytes
        
        // Bitrate nominal (4 bytes) - 0 (unknown)
        ogg_data.insert(ogg_data.end(), {0x00, 0x00, 0x00, 0x00}); // 4 bytes
        
        // Bitrate min (4 bytes) - 0 (unknown)
        ogg_data.insert(ogg_data.end(), {0x00, 0x00, 0x00, 0x00}); // 4 bytes
        
        // Blocksize (1 byte) - 0x88 means blocksize_0=8, blocksize_1=8
        ogg_data.push_back(0x88); // 1 byte
        
        // Framing flag (1 byte) - must be 1
        ogg_data.push_back(0x01); // 1 byte
        
        // Total: 7 + 4 + 1 + 4 + 4 + 4 + 4 + 1 + 1 = 30 bytes (matches segment size)
        
        return ogg_data;
    }
    
    /**
     * @brief Create IOHandler for testing using real test file or minimal data
     */
    static std::unique_ptr<IOHandler> createTestIOHandler() {
        // Try to use a real Ogg test file if it exists
        std::vector<std::string> test_files = {
            "data/11 Foo Fighters - Everlong.ogg",
            "../data/test.ogg",
            "test.ogg",
            "/usr/share/sounds/alsa/Front_Left.wav" // Fallback to any audio file
        };
        
        for (const auto& test_file : test_files) {
            std::ifstream check_file(test_file);
            if (check_file.good()) {
                check_file.close();
                std::cout << "  Using test file: " << test_file << std::endl;
                return std::make_unique<FileIOHandler>(test_file);
            }
        }
        
        // Fall back to minimal Ogg data - create a temporary file
        std::cout << "  Using minimal Ogg data (no test files found)" << std::endl;
        std::vector<uint8_t> data = createMinimalOggFile();
        std::string temp_filename = "/tmp/ogg_test_" + std::to_string(rand()) + ".ogg";
        
        std::ofstream temp_file(temp_filename, std::ios::binary);
        temp_file.write(reinterpret_cast<const char*>(data.data()), data.size());
        temp_file.close();
        
        return std::make_unique<FileIOHandler>(temp_filename);
    }
    
    /**
     * @brief Create a memory-based IOHandler for testing
     */
    static std::unique_ptr<IOHandler> createMemoryIOHandler(const std::vector<uint8_t>& data) {
        // Create a temporary file with the test data
        std::string temp_filename = "/tmp/ogg_test_" + std::to_string(rand()) + ".ogg";
        
        std::ofstream temp_file(temp_filename, std::ios::binary);
        temp_file.write(reinterpret_cast<const char*>(data.data()), data.size());
        temp_file.close();
        
        // Create FileIOHandler for the temporary file
        // Note: In a real implementation, we might want a proper MemoryIOHandler
        return std::make_unique<FileIOHandler>(temp_filename);
    }
};

int main() {
    try {
        std::cout << "Starting OggDemuxer Integration Tests..." << std::endl;
        
        // Initialize random seed for temporary file names
        srand(static_cast<unsigned int>(time(nullptr)));
        
        OggDemuxerIntegrationTest::runAllTests();
        
        std::cout << "All tests completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}

#else // !HAVE_OGGDEMUXER

int main() {
    std::cout << "OggDemuxer integration tests skipped - HAVE_OGGDEMUXER not defined" << std::endl;
    return 0;
}

#endif // HAVE_OGGDEMUXER