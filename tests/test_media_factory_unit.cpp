/*
 * test_media_factory_unit.cpp - Unit tests for MediaFactory
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
 * @brief Test MediaFormat data structure
 */
class MediaFormatTest : public TestCase {
public:
    MediaFormatTest() : TestCase("MediaFormat Data Structure Test") {}
    
protected:
    void runTest() override {
        // Test default constructor
        MediaFormat format1;
        ASSERT_TRUE(format1.format_id.empty(), "Default format_id should be empty");
        ASSERT_TRUE(format1.display_name.empty(), "Default display_name should be empty");
        ASSERT_TRUE(format1.extensions.empty(), "Default extensions should be empty");
        ASSERT_TRUE(format1.mime_types.empty(), "Default mime_types should be empty");
        ASSERT_EQUALS(100, format1.priority, "Default priority should be 100");
        ASSERT_FALSE(format1.supports_streaming, "Default supports_streaming should be false");
        ASSERT_TRUE(format1.supports_seeking, "Default supports_seeking should be true");
        ASSERT_FALSE(format1.is_container, "Default is_container should be false");
        
        // Test populated format
        MediaFormat format2;
        format2.format_id = "mp3";
        format2.display_name = "MPEG Audio Layer 3";
        format2.extensions = {"mp3", "mp2", "mp1"};
        format2.mime_types = {"audio/mpeg", "audio/mp3"};
        format2.magic_signatures = {"ID3", "FFFB", "FFF3"};
        format2.priority = 80;
        format2.supports_streaming = true;
        format2.supports_seeking = true;
        format2.is_container = false;
        format2.description = "MPEG-1/2 Audio Layer III";
        
        ASSERT_EQUALS("mp3", format2.format_id, "Format ID should be set correctly");
        ASSERT_EQUALS("MPEG Audio Layer 3", format2.display_name, "Display name should be set correctly");
        ASSERT_EQUALS(3u, format2.extensions.size(), "Extensions count should be correct");
        ASSERT_EQUALS("mp3", format2.extensions[0], "First extension should be correct");
        ASSERT_EQUALS(2u, format2.mime_types.size(), "MIME types count should be correct");
        ASSERT_EQUALS("audio/mpeg", format2.mime_types[0], "First MIME type should be correct");
        ASSERT_EQUALS(3u, format2.magic_signatures.size(), "Magic signatures count should be correct");
        ASSERT_EQUALS(80, format2.priority, "Priority should be set correctly");
        ASSERT_TRUE(format2.supports_streaming, "Streaming support should be set correctly");
        ASSERT_TRUE(format2.supports_seeking, "Seeking support should be set correctly");
        ASSERT_FALSE(format2.is_container, "Container flag should be set correctly");
        ASSERT_EQUALS("MPEG-1/2 Audio Layer III", format2.description, "Description should be set correctly");
    }
};

/**
 * @brief Test ContentInfo data structure
 */
class ContentInfoTest : public TestCase {
public:
    ContentInfoTest() : TestCase("ContentInfo Data Structure Test") {}
    
protected:
    void runTest() override {
        // Test default constructor
        ContentInfo info1;
        ASSERT_TRUE(info1.detected_format.empty(), "Default detected_format should be empty");
        ASSERT_TRUE(info1.mime_type.empty(), "Default mime_type should be empty");
        ASSERT_TRUE(info1.file_extension.empty(), "Default file_extension should be empty");
        ASSERT_EQUALS(0.0f, info1.confidence, "Default confidence should be 0.0");
        ASSERT_TRUE(info1.metadata.empty(), "Default metadata should be empty");
        
        // Test populated content info
        ContentInfo info2;
        info2.detected_format = "ogg";
        info2.mime_type = "audio/ogg";
        info2.file_extension = "ogg";
        info2.confidence = 0.95f;
        info2.metadata["codec"] = "vorbis";
        info2.metadata["bitrate"] = "192";
        
        ASSERT_EQUALS("ogg", info2.detected_format, "Detected format should be set correctly");
        ASSERT_EQUALS("audio/ogg", info2.mime_type, "MIME type should be set correctly");
        ASSERT_EQUALS("ogg", info2.file_extension, "File extension should be set correctly");
        ASSERT_EQUALS(0.95f, info2.confidence, "Confidence should be set correctly");
        ASSERT_EQUALS(2u, info2.metadata.size(), "Metadata count should be correct");
        ASSERT_EQUALS("vorbis", info2.metadata["codec"], "Codec metadata should be correct");
        ASSERT_EQUALS("192", info2.metadata["bitrate"], "Bitrate metadata should be correct");
    }
};

/**
 * @brief Test URI and path utilities
 */
class URIUtilitiesTest : public TestCase {
public:
    URIUtilitiesTest() : TestCase("URI Utilities Test") {}
    
protected:
    void runTest() override {
        // Test extension extraction
        ASSERT_EQUALS("mp3", MediaFactory::extractExtension("file.mp3"), "Simple extension should be extracted");
        ASSERT_EQUALS("ogg", MediaFactory::extractExtension("/path/to/file.ogg"), "Path extension should be extracted");
        ASSERT_EQUALS("flac", MediaFactory::extractExtension("http://example.com/music.flac"), "URL extension should be extracted");
        ASSERT_EQUALS("wav", MediaFactory::extractExtension("file.name.with.dots.wav"), "Multiple dots should work");
        ASSERT_TRUE(MediaFactory::extractExtension("file_no_extension").empty(), "No extension should return empty");
        ASSERT_TRUE(MediaFactory::extractExtension("file.").empty(), "Empty extension should return empty");
        ASSERT_TRUE(MediaFactory::extractExtension(".hidden").empty(), "Hidden file should return empty");
        
        // Test HTTP URI detection
        ASSERT_TRUE(MediaFactory::isHttpUri("http://example.com/file.mp3"), "HTTP URI should be detected");
        ASSERT_TRUE(MediaFactory::isHttpUri("https://example.com/file.mp3"), "HTTPS URI should be detected");
        ASSERT_FALSE(MediaFactory::isHttpUri("file:///path/to/file.mp3"), "File URI should not be HTTP");
        ASSERT_FALSE(MediaFactory::isHttpUri("/local/path/file.mp3"), "Local path should not be HTTP");
        ASSERT_FALSE(MediaFactory::isHttpUri("ftp://example.com/file.mp3"), "FTP URI should not be HTTP");
        
        // Test local file detection
        ASSERT_TRUE(MediaFactory::isLocalFile("/absolute/path/file.mp3"), "Absolute path should be local file");
        ASSERT_TRUE(MediaFactory::isLocalFile("relative/path/file.mp3"), "Relative path should be local file");
        ASSERT_TRUE(MediaFactory::isLocalFile("file.mp3"), "Simple filename should be local file");
        ASSERT_TRUE(MediaFactory::isLocalFile("file:///path/to/file.mp3"), "File URI should be local file");
        ASSERT_FALSE(MediaFactory::isLocalFile("http://example.com/file.mp3"), "HTTP URI should not be local file");
        ASSERT_FALSE(MediaFactory::isLocalFile("https://example.com/file.mp3"), "HTTPS URI should not be local file");
    }
};

/**
 * @brief Test MIME type utilities
 */
class MimeTypeUtilitiesTest : public TestCase {
public:
    MimeTypeUtilitiesTest() : TestCase("MIME Type Utilities Test") {}
    
protected:
    void runTest() override {
        // Test extension to MIME type conversion
        std::string mp3_mime = MediaFactory::extensionToMimeType("mp3");
        ASSERT_FALSE(mp3_mime.empty(), "MP3 extension should have MIME type");
        
        std::string ogg_mime = MediaFactory::extensionToMimeType("ogg");
        ASSERT_FALSE(ogg_mime.empty(), "OGG extension should have MIME type");
        
        std::string wav_mime = MediaFactory::extensionToMimeType("wav");
        ASSERT_FALSE(wav_mime.empty(), "WAV extension should have MIME type");
        
        std::string unknown_mime = MediaFactory::extensionToMimeType("xyz");
        ASSERT_TRUE(unknown_mime.empty(), "Unknown extension should return empty MIME type");
        
        // Test MIME type to extension conversion
        std::string mpeg_ext = MediaFactory::mimeTypeToExtension("audio/mpeg");
        ASSERT_FALSE(mpeg_ext.empty(), "MPEG MIME type should have extension");
        
        std::string ogg_ext = MediaFactory::mimeTypeToExtension("audio/ogg");
        ASSERT_FALSE(ogg_ext.empty(), "OGG MIME type should have extension");
        
        std::string wav_ext = MediaFactory::mimeTypeToExtension("audio/wav");
        ASSERT_FALSE(wav_ext.empty(), "WAV MIME type should have extension");
        
        std::string unknown_ext = MediaFactory::mimeTypeToExtension("application/unknown");
        ASSERT_TRUE(unknown_ext.empty(), "Unknown MIME type should return empty extension");
        
        // Test multiple extensions for MIME type
        auto mpeg_extensions = MediaFactory::getExtensionsForMimeType("audio/mpeg");
        ASSERT_FALSE(mpeg_extensions.empty(), "MPEG MIME type should have extensions");
        
        // Test multiple MIME types for extension
        auto mp3_mimes = MediaFactory::getMimeTypesForExtension("mp3");
        ASSERT_FALSE(mp3_mimes.empty(), "MP3 extension should have MIME types");
    }
};

/**
 * @brief Test format support queries
 */
class FormatSupportTest : public TestCase {
public:
    FormatSupportTest() : TestCase("Format Support Test") {}
    
protected:
    void runTest() override {
        // Test supported formats query
        auto formats = MediaFactory::getSupportedFormats();
        ASSERT_FALSE(formats.empty(), "Should have supported formats");
        
        // Check for common formats
        bool has_ogg = false, has_riff = false, has_mp4 = false;
        for (const auto& format : formats) {
            if (format.format_id == "ogg") has_ogg = true;
            if (format.format_id == "riff") has_riff = true;
            if (format.format_id == "mp4") has_mp4 = true;
        }
        
        ASSERT_TRUE(has_ogg, "Should support OGG format");
        ASSERT_TRUE(has_riff, "Should support RIFF format");
        ASSERT_TRUE(has_mp4, "Should support MP4 format");
        
        // Test format support queries
        ASSERT_TRUE(MediaFactory::supportsFormat("ogg"), "Should support OGG format");
        ASSERT_TRUE(MediaFactory::supportsFormat("riff"), "Should support RIFF format");
        ASSERT_FALSE(MediaFactory::supportsFormat("unknown"), "Should not support unknown format");
        
        // Test extension support queries
        ASSERT_TRUE(MediaFactory::supportsExtension("ogg"), "Should support OGG extension");
        ASSERT_TRUE(MediaFactory::supportsExtension("wav"), "Should support WAV extension");
        ASSERT_TRUE(MediaFactory::supportsExtension("mp3"), "Should support MP3 extension");
        ASSERT_FALSE(MediaFactory::supportsExtension("xyz"), "Should not support unknown extension");
        
        // Test MIME type support queries
        ASSERT_TRUE(MediaFactory::supportsMimeType("audio/ogg"), "Should support OGG MIME type");
        ASSERT_TRUE(MediaFactory::supportsMimeType("audio/wav"), "Should support WAV MIME type");
        ASSERT_FALSE(MediaFactory::supportsMimeType("application/unknown"), "Should not support unknown MIME type");
        
        // Test streaming support queries
        ASSERT_TRUE(MediaFactory::supportsStreaming("ogg"), "OGG should support streaming");
        ASSERT_TRUE(MediaFactory::supportsStreaming("mp4"), "MP4 should support streaming");
    }
};

/**
 * @brief Test format information queries
 */
class FormatInfoTest : public TestCase {
public:
    FormatInfoTest() : TestCase("Format Info Test") {}
    
protected:
    void runTest() override {
        // Test getting format info for known format
        auto ogg_info = MediaFactory::getFormatInfo("ogg");
        ASSERT_TRUE(ogg_info.has_value(), "Should have OGG format info");
        if (ogg_info) {
            ASSERT_EQUALS("ogg", ogg_info->format_id, "Format ID should match");
            ASSERT_FALSE(ogg_info->display_name.empty(), "Display name should not be empty");
            ASSERT_FALSE(ogg_info->extensions.empty(), "Extensions should not be empty");
        }
        
        // Test getting format info for RIFF format
        auto riff_info = MediaFactory::getFormatInfo("riff");
        ASSERT_TRUE(riff_info.has_value(), "Should have RIFF format info");
        if (riff_info) {
            ASSERT_EQUALS("riff", riff_info->format_id, "Format ID should match");
            ASSERT_FALSE(riff_info->display_name.empty(), "Display name should not be empty");
        }
        
        // Test getting format info for unknown format
        auto unknown_info = MediaFactory::getFormatInfo("unknown");
        ASSERT_FALSE(unknown_info.has_value(), "Should not have unknown format info");
    }
};

/**
 * @brief Test content analysis
 */
class ContentAnalysisTest : public TestCase {
public:
    ContentAnalysisTest() : TestCase("Content Analysis Test") {}
    
protected:
    void runTest() override {
        // Test content analysis by extension
        ContentInfo mp3_info = MediaFactory::analyzeContent("test.mp3");
        ASSERT_FALSE(mp3_info.detected_format.empty(), "MP3 file should be detected");
        ASSERT_EQUALS("mp3", mp3_info.file_extension, "File extension should be extracted");
        ASSERT_TRUE(mp3_info.confidence > 0.0f, "Should have some confidence");
        
        // Test content analysis by URL
        ContentInfo url_info = MediaFactory::analyzeContent("http://example.com/stream.ogg");
        ASSERT_FALSE(url_info.detected_format.empty(), "OGG URL should be detected");
        ASSERT_EQUALS("ogg", url_info.file_extension, "File extension should be extracted from URL");
        
        // Test content analysis for unknown extension
        ContentInfo unknown_info = MediaFactory::analyzeContent("test.xyz");
        ASSERT_TRUE(unknown_info.detected_format.empty(), "Unknown extension should not be detected");
        ASSERT_EQUALS("xyz", unknown_info.file_extension, "File extension should still be extracted");
        ASSERT_EQUALS(0.0f, unknown_info.confidence, "Confidence should be 0 for unknown format");
    }
};

/**
 * @brief Test format registration
 */
class FormatRegistrationTest : public TestCase {
public:
    FormatRegistrationTest() : TestCase("Format Registration Test") {}
    
protected:
    void runTest() override {
        // Get initial format count
        auto initial_formats = MediaFactory::getSupportedFormats();
        size_t initial_count = initial_formats.size();
        
        // Create a custom format
        MediaFormat custom_format;
        custom_format.format_id = "test_format";
        custom_format.display_name = "Test Format";
        custom_format.extensions = {"test", "tst"};
        custom_format.mime_types = {"audio/test"};
        custom_format.priority = 50;
        custom_format.supports_streaming = true;
        custom_format.supports_seeking = true;
        custom_format.description = "Test format for unit testing";
        
        // Create a custom stream factory
        auto custom_factory = [](const std::string& uri, const ContentInfo& info) -> std::unique_ptr<Stream> {
            return nullptr; // Return nullptr for testing
        };
        
        // Register the custom format
        MediaFactory::registerFormat(custom_format, custom_factory);
        
        // Check that format was registered
        auto updated_formats = MediaFactory::getSupportedFormats();
        ASSERT_EQUALS(initial_count + 1, updated_formats.size(), "Format count should increase by 1");
        
        // Test format support queries
        ASSERT_TRUE(MediaFactory::supportsFormat("test_format"), "Should support registered format");
        ASSERT_TRUE(MediaFactory::supportsExtension("test"), "Should support registered extension");
        ASSERT_TRUE(MediaFactory::supportsMimeType("audio/test"), "Should support registered MIME type");
        ASSERT_TRUE(MediaFactory::supportsStreaming("test_format"), "Should support streaming for registered format");
        
        // Test format info query
        auto custom_info = MediaFactory::getFormatInfo("test_format");
        ASSERT_TRUE(custom_info.has_value(), "Should have custom format info");
        if (custom_info) {
            ASSERT_EQUALS("test_format", custom_info->format_id, "Format ID should match");
            ASSERT_EQUALS("Test Format", custom_info->display_name, "Display name should match");
            ASSERT_EQUALS(2u, custom_info->extensions.size(), "Extensions count should match");
            ASSERT_EQUALS("test", custom_info->extensions[0], "First extension should match");
        }
        
        // Test content analysis with registered format
        ContentInfo test_info = MediaFactory::analyzeContent("file.test");
        ASSERT_EQUALS("test_format", test_info.detected_format, "Registered format should be detected");
        ASSERT_EQUALS("test", test_info.file_extension, "Extension should be extracted");
        ASSERT_TRUE(test_info.confidence > 0.0f, "Should have confidence for registered format");
        
        // Test unregistration
        MediaFactory::unregisterFormat("test_format");
        ASSERT_FALSE(MediaFactory::supportsFormat("test_format"), "Should not support unregistered format");
        ASSERT_FALSE(MediaFactory::supportsExtension("test"), "Should not support unregistered extension");
    }
};

/**
 * @brief Test content detector registration
 */
class ContentDetectorTest : public TestCase {
public:
    ContentDetectorTest() : TestCase("Content Detector Test") {}
    
protected:
    void runTest() override {
        // Register a custom content detector
        bool detector_called = false;
        auto custom_detector = [&detector_called](std::unique_ptr<IOHandler>& handler) -> std::optional<ContentInfo> {
            detector_called = true;
            ContentInfo info;
            info.detected_format = "custom_detected";
            info.confidence = 0.8f;
            return info;
        };
        
        // First register a format for the detector
        MediaFormat custom_format;
        custom_format.format_id = "custom_detected";
        custom_format.display_name = "Custom Detected Format";
        custom_format.extensions = {"cdf"};
        
        auto custom_factory = [](const std::string& uri, const ContentInfo& info) -> std::unique_ptr<Stream> {
            return nullptr;
        };
        
        MediaFactory::registerFormat(custom_format, custom_factory);
        MediaFactory::registerContentDetector("custom_detected", custom_detector);
        
        // The detector would be called during content analysis with IOHandler
        // This is a simplified test since we can't easily create IOHandler here
        ASSERT_TRUE(MediaFactory::supportsFormat("custom_detected"), "Custom detected format should be supported");
        
        // Cleanup
        MediaFactory::unregisterFormat("custom_detected");
    }
};

int main() {
    TestSuite suite("MediaFactory Unit Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<MediaFormatTest>());
    suite.addTest(std::make_unique<ContentInfoTest>());
    suite.addTest(std::make_unique<URIUtilitiesTest>());
    suite.addTest(std::make_unique<MimeTypeUtilitiesTest>());
    suite.addTest(std::make_unique<FormatSupportTest>());
    suite.addTest(std::make_unique<FormatInfoTest>());
    suite.addTest(std::make_unique<ContentAnalysisTest>());
    suite.addTest(std::make_unique<FormatRegistrationTest>());
    suite.addTest(std::make_unique<ContentDetectorTest>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}