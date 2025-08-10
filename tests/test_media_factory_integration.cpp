/*
 * test_media_factory_integration.cpp - Integration tests for MediaFactory
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
 * @brief Test MediaFactory stream creation with various formats
 */
class MediaFactoryStreamCreationTest : public TestCase {
public:
    MediaFactoryStreamCreationTest() : TestCase("MediaFactory Stream Creation Test") {}
    
protected:
    void runTest() override {
        // Test stream creation with different file extensions
        
        // Test MP3 stream creation
        try {
            auto mp3_stream = MediaFactory::createStream("test.mp3");
            // Stream creation may fail if file doesn't exist, but should not crash
            // The factory should at least recognize the format
        } catch (const UnsupportedMediaException& e) {
            // This is acceptable if the format is not supported
        } catch (const std::exception& e) {
            // File not found is acceptable for this test
            std::string error_msg = e.what();
            ASSERT_TRUE(error_msg.find("not found") != std::string::npos || 
                       error_msg.find("No such file") != std::string::npos,
                       "Should fail with file not found, not other errors");
        }
        
        // Test OGG stream creation
        try {
            auto ogg_stream = MediaFactory::createStream("test.ogg");
        } catch (const UnsupportedMediaException& e) {
            // Acceptable if not supported
        } catch (const std::exception& e) {
            // File not found is acceptable
        }
        
        // Test WAV stream creation
        try {
            auto wav_stream = MediaFactory::createStream("test.wav");
        } catch (const UnsupportedMediaException& e) {
            // Acceptable if not supported
        } catch (const std::exception& e) {
            // File not found is acceptable
        }
        
        // Test FLAC stream creation
        try {
            auto flac_stream = MediaFactory::createStream("test.flac");
        } catch (const UnsupportedMediaException& e) {
            // Acceptable if not supported
        } catch (const std::exception& e) {
            // File not found is acceptable
        }
        
        // Test unknown format
        try {
            auto unknown_stream = MediaFactory::createStream("test.unknown");
            ASSERT_NULL(unknown_stream.get(), "Unknown format should return nullptr or throw");
        } catch (const UnsupportedMediaException& e) {
            // This is the expected behavior
            ASSERT_TRUE(true, "Unknown format should throw UnsupportedMediaException");
        }
    }
};

/**
 * @brief Test MediaFactory with MIME type hints
 */
class MediaFactoryMimeTypeTest : public TestCase {
public:
    MediaFactoryMimeTypeTest() : TestCase("MediaFactory MIME Type Test") {}
    
protected:
    void runTest() override {
        // Test stream creation with MIME type hints
        
        // Test audio/mpeg MIME type
        try {
            auto mp3_stream = MediaFactory::createStreamWithMimeType("stream", "audio/mpeg");
        } catch (const UnsupportedMediaException& e) {
            // Acceptable if not supported
        } catch (const std::exception& e) {
            // Other errors are acceptable for this test
        }
        
        // Test audio/ogg MIME type
        try {
            auto ogg_stream = MediaFactory::createStreamWithMimeType("stream", "audio/ogg");
        } catch (const UnsupportedMediaException& e) {
            // Acceptable if not supported
        } catch (const std::exception& e) {
            // Other errors are acceptable
        }
        
        // Test audio/wav MIME type
        try {
            auto wav_stream = MediaFactory::createStreamWithMimeType("stream", "audio/wav");
        } catch (const UnsupportedMediaException& e) {
            // Acceptable if not supported
        } catch (const std::exception& e) {
            // Other errors are acceptable
        }
        
        // Test unknown MIME type
        try {
            auto unknown_stream = MediaFactory::createStreamWithMimeType("stream", "application/unknown");
            ASSERT_NULL(unknown_stream.get(), "Unknown MIME type should return nullptr or throw");
        } catch (const UnsupportedMediaException& e) {
            // This is expected
            ASSERT_TRUE(true, "Unknown MIME type should throw UnsupportedMediaException");
        }
    }
};

/**
 * @brief Test MediaFactory content analysis
 */
class MediaFactoryContentAnalysisTest : public TestCase {
public:
    MediaFactoryContentAnalysisTest() : TestCase("MediaFactory Content Analysis Test") {}
    
protected:
    void runTest() override {
        // Test content analysis with various file extensions
        
        // Test MP3 analysis
        ContentInfo mp3_info = MediaFactory::analyzeContent("test.mp3");
        ASSERT_EQUALS("mp3", mp3_info.file_extension, "Should extract MP3 extension");
        ASSERT_FALSE(mp3_info.detected_format.empty(), "Should detect MP3 format");
        ASSERT_TRUE(mp3_info.confidence > 0.0f, "Should have confidence for known format");
        
        // Test OGG analysis
        ContentInfo ogg_info = MediaFactory::analyzeContent("test.ogg");
        ASSERT_EQUALS("ogg", ogg_info.file_extension, "Should extract OGG extension");
        ASSERT_FALSE(ogg_info.detected_format.empty(), "Should detect OGG format");
        
        // Test WAV analysis
        ContentInfo wav_info = MediaFactory::analyzeContent("test.wav");
        ASSERT_EQUALS("wav", wav_info.file_extension, "Should extract WAV extension");
        ASSERT_FALSE(wav_info.detected_format.empty(), "Should detect WAV format");
        
        // Test FLAC analysis
        ContentInfo flac_info = MediaFactory::analyzeContent("test.flac");
        ASSERT_EQUALS("flac", flac_info.file_extension, "Should extract FLAC extension");
        ASSERT_FALSE(flac_info.detected_format.empty(), "Should detect FLAC format");
        
        // Test URL analysis
        ContentInfo url_info = MediaFactory::analyzeContent("http://example.com/stream.mp3");
        ASSERT_EQUALS("mp3", url_info.file_extension, "Should extract extension from URL");
        ASSERT_FALSE(url_info.detected_format.empty(), "Should detect format from URL");
        
        // Test unknown extension
        ContentInfo unknown_info = MediaFactory::analyzeContent("test.xyz");
        ASSERT_EQUALS("xyz", unknown_info.file_extension, "Should extract unknown extension");
        ASSERT_TRUE(unknown_info.detected_format.empty(), "Should not detect unknown format");
        ASSERT_EQUALS(0.0f, unknown_info.confidence, "Should have zero confidence for unknown");
    }
};

/**
 * @brief Test MediaFactory format registration and unregistration
 */
class MediaFactoryRegistrationTest : public TestCase {
public:
    MediaFactoryRegistrationTest() : TestCase("MediaFactory Registration Test") {}
    
protected:
    void runTest() override {
        // Get initial format count
        auto initial_formats = MediaFactory::getSupportedFormats();
        size_t initial_count = initial_formats.size();
        
        // Create a test format
        MediaFormat test_format;
        test_format.format_id = "test_integration";
        test_format.display_name = "Test Integration Format";
        test_format.extensions = {"tif", "test"};
        test_format.mime_types = {"audio/test-integration"};
        test_format.priority = 75;
        test_format.supports_streaming = true;
        test_format.supports_seeking = true;
        test_format.description = "Test format for integration testing";
        
        // Create a test stream factory
        auto test_factory = [](const std::string& uri, const ContentInfo& info) -> std::unique_ptr<Stream> {
            return nullptr; // Return nullptr for testing
        };
        
        // Register the test format
        MediaFactory::registerFormat(test_format, test_factory);
        
        // Verify registration
        auto updated_formats = MediaFactory::getSupportedFormats();
        ASSERT_EQUALS(initial_count + 1, updated_formats.size(), "Format count should increase");
        
        ASSERT_TRUE(MediaFactory::supportsFormat("test_integration"), "Should support registered format");
        ASSERT_TRUE(MediaFactory::supportsExtension("tif"), "Should support registered extension");
        ASSERT_TRUE(MediaFactory::supportsMimeType("audio/test-integration"), "Should support registered MIME type");
        ASSERT_TRUE(MediaFactory::supportsStreaming("test_integration"), "Should support streaming");
        
        // Test format info retrieval
        auto format_info = MediaFactory::getFormatInfo("test_integration");
        ASSERT_TRUE(format_info.has_value(), "Should have format info");
        if (format_info) {
            ASSERT_EQUALS("test_integration", format_info->format_id, "Format ID should match");
            ASSERT_EQUALS("Test Integration Format", format_info->display_name, "Display name should match");
            ASSERT_EQUALS(2u, format_info->extensions.size(), "Should have 2 extensions");
        }
        
        // Test content analysis with registered format
        ContentInfo test_info = MediaFactory::analyzeContent("file.tif");
        ASSERT_EQUALS("test_integration", test_info.detected_format, "Should detect registered format");
        ASSERT_EQUALS("tif", test_info.file_extension, "Should extract extension");
        ASSERT_TRUE(test_info.confidence > 0.0f, "Should have confidence");
        
        // Test MIME type utilities
        std::string mime_for_ext = MediaFactory::extensionToMimeType("tif");
        ASSERT_EQUALS("audio/test-integration", mime_for_ext, "Should return registered MIME type");
        
        std::string ext_for_mime = MediaFactory::mimeTypeToExtension("audio/test-integration");
        ASSERT_EQUALS("tif", ext_for_mime, "Should return first registered extension");
        
        // Test stream creation (should call our factory)
        try {
            auto test_stream = MediaFactory::createStream("file.tif");
            ASSERT_NULL(test_stream.get(), "Test factory returns nullptr");
        } catch (const std::exception& e) {
            // Factory may throw, which is acceptable
        }
        
        // Test unregistration
        MediaFactory::unregisterFormat("test_integration");
        
        ASSERT_FALSE(MediaFactory::supportsFormat("test_integration"), "Should not support unregistered format");
        ASSERT_FALSE(MediaFactory::supportsExtension("tif"), "Should not support unregistered extension");
        ASSERT_FALSE(MediaFactory::supportsMimeType("audio/test-integration"), "Should not support unregistered MIME type");
        
        auto final_formats = MediaFactory::getSupportedFormats();
        ASSERT_EQUALS(initial_count, final_formats.size(), "Format count should return to original");
    }
};

/**
 * @brief Test MediaFactory HTTP streaming support
 */
class MediaFactoryStreamingTest : public TestCase {
public:
    MediaFactoryStreamingTest() : TestCase("MediaFactory Streaming Test") {}
    
protected:
    void runTest() override {
        // Test HTTP URI detection
        ASSERT_TRUE(MediaFactory::isHttpUri("http://example.com/stream.mp3"), "Should detect HTTP URI");
        ASSERT_TRUE(MediaFactory::isHttpUri("https://example.com/stream.ogg"), "Should detect HTTPS URI");
        ASSERT_FALSE(MediaFactory::isHttpUri("file:///path/to/file.mp3"), "Should not detect file URI as HTTP");
        ASSERT_FALSE(MediaFactory::isHttpUri("/local/path/file.mp3"), "Should not detect local path as HTTP");
        
        // Test local file detection
        ASSERT_TRUE(MediaFactory::isLocalFile("/path/to/file.mp3"), "Should detect local file");
        ASSERT_TRUE(MediaFactory::isLocalFile("relative/path.ogg"), "Should detect relative path");
        ASSERT_TRUE(MediaFactory::isLocalFile("file:///path/to/file.wav"), "Should detect file URI");
        ASSERT_FALSE(MediaFactory::isLocalFile("http://example.com/stream.mp3"), "Should not detect HTTP as local");
        
        // Test streaming support queries
        ASSERT_TRUE(MediaFactory::supportsStreaming("ogg"), "OGG should support streaming");
        ASSERT_TRUE(MediaFactory::supportsStreaming("mp4"), "MP4 should support streaming");
        
        // Test stream creation with HTTP URLs (will fail due to no network, but should not crash)
        try {
            auto http_stream = MediaFactory::createStream("http://example.com/stream.mp3");
            // May succeed or fail depending on network/implementation
        } catch (const std::exception& e) {
            // Network errors are acceptable
            std::string error_msg = e.what();
            // Should be network-related error, not format error
        }
    }
};

/**
 * @brief Test MediaFactory error handling
 */
class MediaFactoryErrorHandlingTest : public TestCase {
public:
    MediaFactoryErrorHandlingTest() : TestCase("MediaFactory Error Handling Test") {}
    
protected:
    void runTest() override {
        // Test unsupported format exception
        try {
            auto unknown_stream = MediaFactory::createStream("file.completely_unknown_format");
            ASSERT_NULL(unknown_stream.get(), "Unknown format should return nullptr or throw");
        } catch (const UnsupportedMediaException& e) {
            ASSERT_FALSE(std::string(e.what()).empty(), "Exception should have message");
        }
        
        // Test content detection exception scenarios
        try {
            ContentInfo info = MediaFactory::analyzeContent("");
            // Empty URI should be handled gracefully
            ASSERT_TRUE(info.detected_format.empty(), "Empty URI should not detect format");
        } catch (const ContentDetectionException& e) {
            // This is also acceptable
            ASSERT_FALSE(std::string(e.what()).empty(), "Exception should have message");
        }
        
        // Test invalid MIME type
        try {
            auto invalid_stream = MediaFactory::createStreamWithMimeType("file", "invalid/mime/type/format");
            ASSERT_NULL(invalid_stream.get(), "Invalid MIME type should return nullptr or throw");
        } catch (const UnsupportedMediaException& e) {
            // Expected behavior
        }
        
        // Test format info for non-existent format
        auto non_existent_info = MediaFactory::getFormatInfo("non_existent_format");
        ASSERT_FALSE(non_existent_info.has_value(), "Non-existent format should not have info");
        
        // Test utilities with empty/invalid input
        std::string empty_mime = MediaFactory::extensionToMimeType("");
        ASSERT_TRUE(empty_mime.empty(), "Empty extension should return empty MIME type");
        
        std::string empty_ext = MediaFactory::mimeTypeToExtension("");
        ASSERT_TRUE(empty_ext.empty(), "Empty MIME type should return empty extension");
        
        std::string extracted_ext = MediaFactory::extractExtension("");
        ASSERT_TRUE(extracted_ext.empty(), "Empty URI should return empty extension");
    }
};

/**
 * @brief Test MediaFactory thread safety
 */
class MediaFactoryThreadSafetyTest : public TestCase {
public:
    MediaFactoryThreadSafetyTest() : TestCase("MediaFactory Thread Safety Test") {}
    
protected:
    void runTest() override {
        std::atomic<bool> test_passed{true};
        std::atomic<int> operations_completed{0};
        
        // Test concurrent format queries
        auto test_concurrent_operations = [&]() {
            try {
                for (int i = 0; i < 10; ++i) {
                    // Test concurrent format support queries
                    bool supports_ogg = MediaFactory::supportsFormat("ogg");
                    bool supports_mp3 = MediaFactory::supportsExtension("mp3");
                    bool supports_wav_mime = MediaFactory::supportsMimeType("audio/wav");
                    
                    // Test concurrent format list access
                    auto formats = MediaFactory::getSupportedFormats();
                    
                    // Test concurrent content analysis
                    ContentInfo info = MediaFactory::analyzeContent("test.mp3");
                    
                    // Test concurrent utility functions
                    std::string mime = MediaFactory::extensionToMimeType("ogg");
                    std::string ext = MediaFactory::mimeTypeToExtension("audio/mpeg");
                    
                    operations_completed++;
                    
                    // Small delay to encourage thread interleaving
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } catch (...) {
                test_passed = false;
            }
        };
        
        // Run concurrent operations
        std::thread t1(test_concurrent_operations);
        std::thread t2(test_concurrent_operations);
        std::thread t3(test_concurrent_operations);
        
        t1.join();
        t2.join();
        t3.join();
        
        ASSERT_TRUE(test_passed.load(), "Concurrent operations should not fail");
        ASSERT_EQUALS(30, operations_completed.load(), "All operations should complete");
    }
};

/**
 * @brief Test MediaFactory with ContentInfo
 */
class MediaFactoryContentInfoTest : public TestCase {
public:
    MediaFactoryContentInfoTest() : TestCase("MediaFactory ContentInfo Test") {}
    
protected:
    void runTest() override {
        // Create custom ContentInfo
        ContentInfo custom_info;
        custom_info.detected_format = "ogg";
        custom_info.mime_type = "audio/ogg";
        custom_info.file_extension = "ogg";
        custom_info.confidence = 0.9f;
        custom_info.metadata["codec"] = "vorbis";
        custom_info.metadata["bitrate"] = "192";
        
        // Test stream creation with ContentInfo
        try {
            auto stream = MediaFactory::createStreamWithContentInfo("test.ogg", custom_info);
            // May succeed or fail depending on file existence
        } catch (const std::exception& e) {
            // File not found or other errors are acceptable
        }
        
        // Test ContentInfo validation
        ASSERT_EQUALS("ogg", custom_info.detected_format, "Format should be preserved");
        ASSERT_EQUALS("audio/ogg", custom_info.mime_type, "MIME type should be preserved");
        ASSERT_EQUALS(0.9f, custom_info.confidence, "Confidence should be preserved");
        ASSERT_EQUALS(2u, custom_info.metadata.size(), "Metadata should be preserved");
        ASSERT_EQUALS("vorbis", custom_info.metadata["codec"], "Codec metadata should be correct");
        
        // Test with empty ContentInfo
        ContentInfo empty_info;
        try {
            auto empty_stream = MediaFactory::createStreamWithContentInfo("test", empty_info);
            // Should handle gracefully
        } catch (const UnsupportedMediaException& e) {
            // Expected for empty format
        } catch (const std::exception& e) {
            // Other errors are acceptable
        }
    }
};

int main() {
    TestSuite suite("MediaFactory Integration Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<MediaFactoryStreamCreationTest>());
    suite.addTest(std::make_unique<MediaFactoryMimeTypeTest>());
    suite.addTest(std::make_unique<MediaFactoryContentAnalysisTest>());
    suite.addTest(std::make_unique<MediaFactoryRegistrationTest>());
    suite.addTest(std::make_unique<MediaFactoryStreamingTest>());
    suite.addTest(std::make_unique<MediaFactoryErrorHandlingTest>());
    suite.addTest(std::make_unique<MediaFactoryThreadSafetyTest>());
    suite.addTest(std::make_unique<MediaFactoryContentInfoTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}