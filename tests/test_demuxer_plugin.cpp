/*
 * test_demuxer_plugin.cpp - Unit tests for demuxer plugin architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

class TestDemuxerPlugin : public TestCase {
public:
    TestDemuxerPlugin() : TestCase("DemuxerPlugin") {}
    
protected:
    void runTest() override {
        testPluginManagerSingleton();
        testCustomDemuxerRegistration();
        testCustomDetectorRegistration();
        testExtendedMetadata();
        testPluginStats();
        testPluginSearchPaths();
        testFormatValidation();
    }
    
private:
    void testPluginManagerSingleton() {
        // Test singleton pattern
        auto& manager1 = DemuxerPluginManager::getInstance();
        auto& manager2 = DemuxerPluginManager::getInstance();
        
        ASSERT_TRUE(&manager1 == &manager2, "Plugin manager should be singleton");
    }
    
    void testCustomDemuxerRegistration() {
        auto& manager = DemuxerPluginManager::getInstance();
        
        // Create test format
        MediaFormat test_format;
        test_format.format_id = "test_format";
        test_format.display_name = "Test Format";
        test_format.extensions = {"TST", "TEST"};
        test_format.mime_types = {"audio/test"};
        test_format.priority = 50;
        test_format.supports_streaming = true;
        test_format.supports_seeking = true;
        test_format.description = "Test format for plugin testing";
        
        // Create test factory function
        auto factory_func = [](std::unique_ptr<IOHandler> handler) -> std::unique_ptr<Demuxer> {
            // Return null for test - real implementation would create demuxer
            return nullptr;
        };
        
        // Register custom demuxer
        bool registered = manager.registerCustomDemuxer("test_format", factory_func, test_format);
        ASSERT_TRUE(registered, "Custom demuxer should register successfully");
        
        // Check if format is recognized as plugin format
        bool is_plugin = manager.isPluginFormat("test_format");
        ASSERT_TRUE(is_plugin, "Format should be recognized as plugin format");
        
        // Get custom formats
        auto custom_formats = manager.getCustomFormats();
        bool found = false;
        for (const auto& format : custom_formats) {
            if (format.format_id == "test_format") {
                found = true;
                ASSERT_EQUALS(format.display_name, "Test Format", "Format display name should match");
                ASSERT_EQUALS(format.extensions.size(), 2u, "Format should have 2 extensions");
                break;
            }
        }
        ASSERT_TRUE(found, "Custom format should be in custom formats list");
        
        // Unregister format
        bool unregistered = manager.unregisterCustomFormat("test_format");
        ASSERT_TRUE(unregistered, "Custom format should unregister successfully");
        
        // Verify format is no longer plugin format
        is_plugin = manager.isPluginFormat("test_format");
        ASSERT_FALSE(is_plugin, "Format should no longer be plugin format after unregistration");
    }
    
    void testCustomDetectorRegistration() {
        auto& manager = DemuxerPluginManager::getInstance();
        
        // Create test detector function
        auto detector_func = [](std::unique_ptr<IOHandler>& handler) -> std::optional<ContentInfo> {
            ContentInfo info;
            info.detected_format = "test_detector_format";
            info.confidence = 0.8f;
            return info;
        };
        
        // Register custom detector
        bool registered = manager.registerCustomDetector("test_detector_format", detector_func);
        ASSERT_TRUE(registered, "Custom detector should register successfully");
        
        // Unregister detector
        bool unregistered = manager.unregisterCustomFormat("test_detector_format");
        ASSERT_TRUE(unregistered, "Custom detector should unregister successfully");
    }
    
    void testExtendedMetadata() {
        ExtendedMetadata metadata;
        metadata.format_id = "test_format";
        
        // Test string metadata
        metadata.setString("title", "Test Title");
        ASSERT_EQUALS(metadata.getString("title"), "Test Title", "String metadata should be stored and retrieved");
        ASSERT_EQUALS(metadata.getString("nonexistent", "default"), "default", "Default value should be returned for nonexistent key");
        
        // Test numeric metadata
        metadata.setNumeric("duration", 12345);
        ASSERT_EQUALS(metadata.getNumeric("duration"), 12345L, "Numeric metadata should be stored and retrieved");
        ASSERT_EQUALS(metadata.getNumeric("nonexistent", 999), 999L, "Default value should be returned for nonexistent numeric key");
        
        // Test binary metadata
        std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
        metadata.setBinary("codec_data", test_data);
        auto retrieved_data = metadata.getBinary("codec_data");
        ASSERT_EQUALS(retrieved_data.size(), 4u, "Binary metadata should have correct size");
        ASSERT_EQUALS(retrieved_data[0], static_cast<uint8_t>(0x01), "Binary metadata should be stored correctly");
        
        // Test float metadata
        metadata.setFloat("sample_rate", 44100.0);
        ASSERT_EQUALS(metadata.getFloat("sample_rate"), 44100.0, "Float metadata should be stored and retrieved");
        
        // Test key existence
        ASSERT_TRUE(metadata.hasKey("title"), "Should detect existing string key");
        ASSERT_TRUE(metadata.hasKey("duration"), "Should detect existing numeric key");
        ASSERT_TRUE(metadata.hasKey("codec_data"), "Should detect existing binary key");
        ASSERT_TRUE(metadata.hasKey("sample_rate"), "Should detect existing float key");
        ASSERT_FALSE(metadata.hasKey("nonexistent"), "Should not detect nonexistent key");
        
        // Test get all keys
        auto all_keys = metadata.getAllKeys();
        ASSERT_EQUALS(all_keys.size(), 4u, "Should return all 4 keys");
        
        // Test clear
        metadata.clear();
        ASSERT_FALSE(metadata.hasKey("title"), "Keys should be cleared");
        ASSERT_EQUALS(metadata.getAllKeys().size(), 0u, "All keys should be cleared");
    }
    
    void testExtendedStreamInfo() {
        // Create base StreamInfo
        StreamInfo base_info;
        base_info.stream_id = 1;
        base_info.codec_type = "audio";
        base_info.codec_name = "test_codec";
        base_info.sample_rate = 44100;
        base_info.channels = 2;
        
        // Create ExtendedStreamInfo
        ExtendedStreamInfo extended_info(base_info);
        
        // Verify base properties are preserved
        ASSERT_EQUALS(extended_info.stream_id, 1u, "Stream ID should be preserved");
        ASSERT_EQUALS(extended_info.codec_type, "audio", "Codec type should be preserved");
        ASSERT_EQUALS(extended_info.codec_name, "test_codec", "Codec name should be preserved");
        ASSERT_EQUALS(extended_info.sample_rate, 44100u, "Sample rate should be preserved");
        ASSERT_EQUALS(extended_info.channels, static_cast<uint16_t>(2), "Channels should be preserved");
        
        // Test format-specific metadata
        extended_info.setFormatMetadata("encoder", "Test Encoder v1.0");
        ASSERT_EQUALS(extended_info.getFormatMetadata("encoder"), "Test Encoder v1.0", "Format metadata should be stored");
        ASSERT_TRUE(extended_info.hasFormatMetadata("encoder"), "Should detect format metadata");
        ASSERT_FALSE(extended_info.hasFormatMetadata("nonexistent"), "Should not detect nonexistent format metadata");
    }
    
    void testPluginStats() {
        auto& manager = DemuxerPluginManager::getInstance();
        
        // Get initial stats
        auto initial_stats = manager.getPluginStats();
        
        // Register a custom format to change stats
        MediaFormat test_format;
        test_format.format_id = "stats_test_format";
        test_format.display_name = "Stats Test Format";
        
        auto factory_func = [](std::unique_ptr<IOHandler> handler) -> std::unique_ptr<Demuxer> {
            return nullptr;
        };
        
        manager.registerCustomDemuxer("stats_test_format", factory_func, test_format);
        
        // Get updated stats
        auto updated_stats = manager.getPluginStats();
        
        // Verify stats changed
        ASSERT_EQUALS(updated_stats.total_formats_registered, 
                     initial_stats.total_formats_registered + 1,
                     "Format registration should increment stats");
        
        // Clean up
        manager.unregisterCustomFormat("stats_test_format");
    }
    
    void testPluginSearchPaths() {
        auto& manager = DemuxerPluginManager::getInstance();
        
        // Get initial search paths
        auto initial_paths = manager.getPluginSearchPaths();
        ASSERT_TRUE(initial_paths.size() > 0, "Should have default search paths");
        
        // Set custom search paths
        std::vector<std::string> custom_paths = {"/custom/path1", "/custom/path2"};
        manager.setPluginSearchPaths(custom_paths);
        
        // Verify paths were set
        auto current_paths = manager.getPluginSearchPaths();
        ASSERT_EQUALS(current_paths.size(), 2u, "Should have 2 custom paths");
        ASSERT_EQUALS(current_paths[0], "/custom/path1", "First path should match");
        ASSERT_EQUALS(current_paths[1], "/custom/path2", "Second path should match");
        
        // Restore initial paths
        manager.setPluginSearchPaths(initial_paths);
    }
    
    void testFormatValidation() {
        auto& manager = DemuxerPluginManager::getInstance();
        
        // Test invalid format registration (empty format ID)
        MediaFormat invalid_format;
        invalid_format.format_id = "";  // Invalid
        invalid_format.display_name = "Invalid Format";
        
        auto factory_func = [](std::unique_ptr<IOHandler> handler) -> std::unique_ptr<Demuxer> {
            return nullptr;
        };
        
        bool registered = manager.registerCustomDemuxer("", factory_func, invalid_format);
        ASSERT_FALSE(registered, "Should not register format with empty ID");
        
        // Test invalid factory function
        MediaFormat valid_format;
        valid_format.format_id = "valid_format";
        valid_format.display_name = "Valid Format";
        
        registered = manager.registerCustomDemuxer("valid_format", nullptr, valid_format);
        ASSERT_FALSE(registered, "Should not register format with null factory");
    }
};

int main() {
    TestDemuxerPlugin test;
    auto result = test.run();
    
    if (result.result == TestResult::PASSED) {
        std::cout << "All plugin tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Plugin tests failed: " << result.failure_message << std::endl;
        return 1;
    }
}